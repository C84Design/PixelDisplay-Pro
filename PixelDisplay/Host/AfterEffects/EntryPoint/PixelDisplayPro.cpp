// PixelDisplay Pro — Host/AfterEffects/EntryPoint/PixelDisplayPro.cpp
//
// After Effects host adapter (Layer 1). This is the ONLY code that includes the
// Adobe SDK. It contains no rendering logic: it registers parameters (walking
// the shared ParameterCatalog), builds a ParamSnapshot from the current values,
// wraps AE's buffers as engine ImageViews, and calls the engine.
//
// STATUS: compiled only when PD_AE_SDK_ROOT is configured (the SDK is not
// present in the Linux CI). The engine + host-core + tests build without it.
// This file documents the integration contract; it mirrors the SmartFX + MFR
// flow described in DESIGN.md §2, §8, §10.
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "entry.h"

#include <memory>

#include "Engine/Core/Engine.hpp"
#include "Host/AfterEffects/Parameters/ParameterCatalog.hpp"
#include "Host/AfterEffects/Presets/Presets.hpp"

using namespace pd;

namespace {

// One shared engine per plugin instance. Immutable after creation; render() is
// thread-safe, so all MFR worker threads share it (DESIGN.md §10.3).
std::unique_ptr<Engine> gEngine;

// GlobalSetup: advertise Multi-Frame Rendering + SmartFX, create the engine.
PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data) {
    out_data->my_version = PF_VERSION(1, 0, 0, PF_Stage_DEVELOP, 0);
    out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT |
                           PF_OutFlag_NON_PARAM_VARY;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING;  // MFR opt-in
    auto eng = Engine::create();
    if (!eng) { return PF_Err_INTERNAL_STRUCT_DAMAGED; }
    gEngine = std::move(eng.value());
    return PF_Err_NONE;
}

// ParamsSetup: register every control by walking the shared catalog, so the UI
// and the engine parameters can never drift. (Each ParamType maps to the
// matching PF_ADD_* macro; enums become popups, groups become topics.)
PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data) {
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
                // Build a "|"-separated popup string from the options.
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
            case host::ParamType::Group:  break;  // topic start/end handled around groups
        }
    }
    out_data->num_params = (A_long)host::catalog().size() + 1;  // +1 for the input layer
    return PF_Err_NONE;
}

// Map an AE world (pixel buffer) to an engine ImageView. AE 8/16/32-bit worlds
// map to ARGB8/ARGB16/ARGB32F; the engine reads them directly (PixelAccess.hpp).
ImageView wrapWorld(PF_EffectWorld* world, PF_PixelFormat fmt, ColorSpace space) {
    ImageView v;
    v.data = world->data;
    v.width = world->width;
    v.height = world->height;
    v.rowBytes = world->rowbytes;
    switch (fmt) {
        case PF_PixelFormat_ARGB32:  v.format = PixelFormat::ARGB8;   break;
        case PF_PixelFormat_ARGB64:  v.format = PixelFormat::ARGB16;  break;
        case PF_PixelFormat_ARGB128: v.format = PixelFormat::ARGB32F; break;
        default:                     v.format = PixelFormat::ARGB8;   break;
    }
    v.colorSpace = space;
    return v;
}

// Build a ParamSnapshot from the current AE parameter values. In the full
// adapter this reads each PF_ParamDef in catalog order and assigns into the
// snapshot via the same key mapping the preset serializer uses.
ParamSnapshot buildSnapshot(PF_InData* /*in_data*/, PF_ParamDef* /*params*/[]) {
    ParamSnapshot snap;
    // ... assign snap fields from params[i] following host::catalog() order ...
    return snap;
}

// SmartRender: the MFR-safe render path. Each invocation is independent.
PF_Err SmartRender(PF_InData* in_data, PF_OutData* /*out_data*/,
                   PF_SmartRenderExtra* extra) {
    PF_EffectWorld* input = nullptr;
    PF_EffectWorld* output = nullptr;
    PF_Err err = extra->cb->checkout_layer_pixels(in_data->effect_ref, 0 /*input*/, &input);
    if (err) return err;
    err = extra->cb->checkout_output(in_data->effect_ref, &output);
    if (err) return err;

    ParamSnapshot snap;  // built from the flattened pre-render params in real code

    ImageView in = wrapWorld(input, extra->input->pixel_format, ColorSpace::sRGB);
    ImageView out = wrapWorld(output, extra->input->pixel_format, ColorSpace::sRGB);

    RenderRequest req;
    req.input = in;
    req.output = out;
    req.params = &snap;
    req.time.layerTimeSeconds =
        static_cast<double>(in_data->current_time) / in_data->time_scale;
    req.time.frameRate =
        static_cast<double>(in_data->time_scale) / in_data->local_time_step;

    Status st = gEngine->render(req);
    if (!st) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    return extra->cb->checkin_layer_pixels(in_data->effect_ref, 0);
}

}  // namespace

// Plugin entry point dispatched by After Effects.
extern "C" DllExport PF_Err PluginMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
                                       PF_ParamDef* params[], PF_LayerDef* output,
                                       void* extra) {
    switch (cmd) {
        case PF_Cmd_GLOBAL_SETUP: return GlobalSetup(in_data, out_data);
        case PF_Cmd_PARAMS_SETUP: return ParamsSetup(in_data, out_data);
        case PF_Cmd_SMART_RENDER: return SmartRender(in_data, out_data,
                                                     static_cast<PF_SmartRenderExtra*>(extra));
        default: break;
    }
    return PF_Err_NONE;
}
