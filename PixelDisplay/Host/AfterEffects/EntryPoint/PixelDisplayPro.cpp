// PixelDisplay Pro — Host/AfterEffects/EntryPoint/PixelDisplayPro.cpp
//
// After Effects host adapter (Layer 1). This is the ONLY code that includes the
// Adobe SDK. It contains no rendering logic: it registers parameters (walking
// the shared ParameterCatalog), builds a ParamSnapshot from the current values,
// wraps AE's buffers as engine ImageViews, and calls the engine.
//
// The SmartFX render pipeline (PRE_RENDER + SMART_RENDER) is implemented here.
// With the default parameters ("Enable" off) the engine is a passthrough, so a
// freshly-applied effect reproduces the source frame exactly — the same
// bit-exact copy path the engine uses for a disabled effect (see CpuBackend).
//
// STATUS: compiled only when PD_AE_SDK_ROOT is configured (the SDK is not
// present in the Linux CI). The engine + host-core + tests build without it.
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "entry.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "Engine/Core/Engine.hpp"
#include "Host/AfterEffects/Parameters/ParameterCatalog.hpp"
#include "Host/AfterEffects/Presets/Presets.hpp"

using namespace pd;

namespace {

// The input layer is always parameter index 0; catalog controls follow at 1..N.
constexpr A_long PARAM_INPUT = 0;

// One shared engine per plugin instance. Immutable after creation; render() is
// thread-safe, so all MFR worker threads share it (DESIGN.md §10.3).
std::unique_ptr<Engine> gEngine;

// ---- Setup -----------------------------------------------------------------

// GlobalSetup: advertise Multi-Frame Rendering + SmartFX, create the engine.
PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data) {
    out_data->my_version = PF_VERSION(1, 0, 0, PF_Stage_DEVELOP, 0);
    out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT |
                           PF_OutFlag_NON_PARAM_VARY;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING;  // MFR opt-in
    auto eng = Engine::create();
    if (!eng) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    gEngine = std::move(eng.value());
    return PF_Err_NONE;
}

// GlobalSetdown: release the shared engine.
PF_Err GlobalSetdown(PF_InData* /*in_data*/, PF_OutData* /*out_data*/) {
    gEngine.reset();
    return PF_Err_NONE;
}

// ParamsSetup: register every control by walking the shared catalog, so the UI
// and the engine parameters can never drift. (Each ParamType maps to the
// matching PF_ADD_* macro; enums become popups.)
PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;
    for (const host::ParamInfo& p : host::catalog()) {
        AEFX_CLR_STRUCT(def);
        switch (p.type) {
            case host::ParamType::Float:
                PF_ADD_FLOAT_SLIDERX(p.label.c_str(), p.minValue, p.maxValue, p.minValue,
                                     p.maxValue, p.defaultValue, PF_Precision_THOUSANDTHS,
                                     0, 0, 0);
                break;
            case host::ParamType::Int:
                PF_ADD_SLIDER(p.label.c_str(), (A_long)p.minValue, (A_long)p.maxValue,
                              (A_long)p.minValue, (A_long)p.maxValue, (A_long)p.defaultValue, 0);
                break;
            case host::ParamType::Bool:
                PF_ADD_CHECKBOX(p.label.c_str(), "", (A_long)p.defaultValue != 0, 0, 0);
                break;
            case host::ParamType::Enum: {
                std::string menu;
                for (std::size_t i = 0; i < p.options.size(); ++i) {
                    if (i) menu += "|";
                    menu += p.options[i].label;
                }
                PF_ADD_POPUP(p.label.c_str(), (A_long)p.options.size(),
                             (A_long)p.defaultValue + 1, menu.c_str(), 0);
                break;
            }
            case host::ParamType::Color:  PF_ADD_COLOR(p.label.c_str(), 0, 0, 0, 0); break;
            case host::ParamType::Button: PF_ADD_BUTTON(p.label.c_str(), "Apply",
                                                        0, PF_ParamFlag_SUPERVISE, 0); break;
            case host::ParamType::Group:  break;
        }
    }
    out_data->num_params = (A_long)host::catalog().size() + 1;  // +1 for the input layer
    return err;
}

// ---- Buffer bridging -------------------------------------------------------

// Map an AE world to an engine ImageView. AE 8/16/32-bit worlds are ARGB and
// map to ARGB8/ARGB16/ARGB32F; the engine reads them directly (PixelAccess.hpp).
ImageView wrapWorld(PF_EffectWorld* world, A_long bitdepth, ColorSpace space) {
    ImageView v;
    v.data = world->data;
    v.width = world->width;
    v.height = world->height;
    v.rowBytes = world->rowbytes;
    switch (bitdepth) {
        case 16: v.format = PixelFormat::ARGB16;  break;
        case 32: v.format = PixelFormat::ARGB32F; break;
        case 8:
        default: v.format = PixelFormat::ARGB8;   break;
    }
    v.colorSpace = space;
    return v;
}

// Defensive raw copy of the overlapping region (used only if the engine rejects
// the buffers, so the output is never left uninitialized).
void copyWorld(const PF_EffectWorld* src, PF_EffectWorld* dst) {
    if (!src->data || !dst->data) return;
    A_long rows = src->height < dst->height ? src->height : dst->height;
    A_long bytes = src->rowbytes < dst->rowbytes ? src->rowbytes : dst->rowbytes;
    if (bytes < 0) bytes = 0;
    for (A_long y = 0; y < rows; ++y) {
        const char* s = reinterpret_cast<const char*>(src->data) + (size_t)y * src->rowbytes;
        char* d = reinterpret_cast<char*>(dst->data) + (size_t)y * dst->rowbytes;
        std::memcpy(d, s, (size_t)bytes);
    }
}

// ---- Parameter → snapshot --------------------------------------------------

// Read the current parameter values and build a ParamSnapshot. The catalog id of
// each control matches the key used by the preset (de)serializer, so we flatten
// the values into that key=value text format and let host::deserialize apply
// them to the snapshot — reusing the single field mapping, no duplication.
ParamSnapshot buildSnapshot(PF_InData* in_data) {
    ParamSnapshot snap;  // defaults: display.enable == false => passthrough
    const auto& cat = host::catalog();

    std::string text;
    text.reserve(cat.size() * 24);
    char line[160];

    bool haveDeadColor = false;
    float deadR = 0, deadG = 0, deadB = 0;

    for (std::size_t i = 0; i < cat.size(); ++i) {
        const host::ParamInfo& p = cat[i];
        if (p.type == host::ParamType::Button || p.type == host::ParamType::Group) continue;

        PF_ParamDef def;
        AEFX_CLR_STRUCT(def);
        PF_Err err = PF_CHECKOUT_PARAM(in_data, (A_long)(i + 1), in_data->current_time,
                                       in_data->time_step, in_data->time_scale, &def);
        if (err) continue;

        double value = 0.0;
        bool emit = true;
        switch (p.type) {
            case host::ParamType::Bool:  value = def.u.bd.value ? 1.0 : 0.0; break;
            case host::ParamType::Float: value = (double)def.u.fs_d.value; break;
            case host::ParamType::Int:   value = (double)def.u.sd.value; break;
            case host::ParamType::Enum:  value = (double)(def.u.pd.value - 1); break;  // 0-based
            case host::ParamType::Color:
                deadR = def.u.cd.value.red   / 255.0f;
                deadG = def.u.cd.value.green / 255.0f;
                deadB = def.u.cd.value.blue  / 255.0f;
                haveDeadColor = true;
                emit = false;
                break;
            default: emit = false; break;
        }
        if (emit) {
            std::snprintf(line, sizeof line, "%s=%.9g\n", p.id.c_str(), value);
            text += line;
        }
        PF_CHECKIN_PARAM(in_data, &def);
    }

    host::deserialize(text, snap);  // applies every recognised key to the snapshot
    if (haveDeadColor) {
        snap.artifacts.deadPixelColor.x = deadR;
        snap.artifacts.deadPixelColor.y = deadG;
        snap.artifacts.deadPixelColor.z = deadB;
    }
    return snap;
}

// ---- SmartFX render --------------------------------------------------------

void DeletePreRenderData(void* pre_render_dataPV) {
    delete static_cast<ParamSnapshot*>(pre_render_dataPV);
}

// PreRender: flatten the parameters into a snapshot (consumed later on possibly
// other MFR threads via pre_render_data) and check out the input layer so
// SmartRender can read its pixels.
PF_Err PreRender(PF_InData* in_data, PF_OutData* /*out_data*/, PF_PreRenderExtra* extra) {
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;

    ParamSnapshot* snap = new (std::nothrow) ParamSnapshot(buildSnapshot(in_data));
    if (!snap) return PF_Err_OUT_OF_MEMORY;
    extra->output->pre_render_data = snap;
    extra->output->delete_pre_render_data_func = DeletePreRenderData;

    err = extra->cb->checkout_layer(in_data->effect_ref, PARAM_INPUT, PARAM_INPUT,
                                    &req, in_data->current_time, in_data->time_step,
                                    in_data->time_scale, &in_result);
    if (!err) {
        // 1:1 point effect: the output covers the input's result region.
        extra->output->result_rect = in_result.result_rect;
        extra->output->max_result_rect = in_result.max_result_rect;
    }
    return err;
}

// SmartRender: the MFR-safe render path. Each invocation is independent and uses
// the snapshot flattened in PreRender.
PF_Err SmartRender(PF_InData* in_data, PF_OutData* /*out_data*/, PF_SmartRenderExtra* extra) {
    PF_Err err = PF_Err_NONE;
    PF_EffectWorld* input = nullptr;
    PF_EffectWorld* output = nullptr;

    const ParamSnapshot* snap =
        static_cast<const ParamSnapshot*>(extra->input->pre_render_data);
    ParamSnapshot fallback;  // defaults => passthrough, if pre-render data is missing
    const ParamSnapshot* params = snap ? snap : &fallback;

    err = extra->cb->checkout_layer_pixels(in_data->effect_ref, PARAM_INPUT, &input);
    if (err) return err;

    err = extra->cb->checkout_output(in_data->effect_ref, &output);
    if (!err && input && output) {
        const A_long bitdepth = extra->input->bitdepth;
        ImageView in  = wrapWorld(input,  bitdepth, ColorSpace::sRGB);
        ImageView out = wrapWorld(output, bitdepth, ColorSpace::sRGB);

        RenderRequest rreq;
        rreq.input = in;
        rreq.output = out;
        rreq.params = params;
        rreq.time.layerTimeSeconds =
            in_data->time_scale ? (double)in_data->current_time / in_data->time_scale : 0.0;
        rreq.time.frameRate =
            in_data->local_time_step ? (double)in_data->time_scale / in_data->local_time_step
                                     : 30.0;

        bool ok = false;
        if (gEngine) {
            Status st = gEngine->render(rreq);
            ok = static_cast<bool>(st);
        }
        if (!ok) copyWorld(input, output);  // never leave the output uninitialized
    }

    (void)extra->cb->checkin_layer_pixels(in_data->effect_ref, PARAM_INPUT);
    return err;
}

}  // namespace

// Plugin entry point dispatched by After Effects.
extern "C" DllExport PF_Err PluginMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
                                       PF_ParamDef* params[], PF_LayerDef* output,
                                       void* extra) {
    (void)params;
    (void)output;
    PF_Err err = PF_Err_NONE;
    switch (cmd) {
        case PF_Cmd_GLOBAL_SETUP:     err = GlobalSetup(in_data, out_data); break;
        case PF_Cmd_GLOBAL_SETDOWN:   err = GlobalSetdown(in_data, out_data); break;
        case PF_Cmd_PARAMS_SETUP:     err = ParamsSetup(in_data, out_data); break;
        case PF_Cmd_SMART_PRE_RENDER:
            err = PreRender(in_data, out_data, static_cast<PF_PreRenderExtra*>(extra));
            break;
        case PF_Cmd_SMART_RENDER:
            err = SmartRender(in_data, out_data, static_cast<PF_SmartRenderExtra*>(extra));
            break;
        default: break;
    }
    return err;
}
