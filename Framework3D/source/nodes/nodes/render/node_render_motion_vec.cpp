﻿#include "NODES_FILES_DIR.h"
#include "Nodes/node.hpp"
#include "Nodes/node_declare.hpp"
#include "Nodes/node_register.h"
#include "RCore/Backend.hpp"
#include "Utils/Math/math.h"
#include "nvrhi/utils.h"
#include "render_node_base.h"
#include "resource_allocator_instance.hpp"
#include "shaders/utils/motion_vec_cb.h"
#include "utils/compile_shader.h"

namespace USTC_CG::node_render_motion_vec {

struct PrevCamStatus {
    pxr::GfMatrix4f PrevProjViewMatrix;
};

static void node_declare(NodeDeclarationBuilder& b)
{
    b.add_input<decl::Texture>("World Position");
    b.add_input<decl::Camera>("Camera");
    b.add_output<decl::Texture>("Motion Vector");

    b.add_runtime_storage<PrevCamStatus>();
}

static void node_exec(ExeParams params)
{
    auto world_position = params.get_input<TextureHandle>("World Position");
    auto texture_info = world_position->getDesc();
    Hd_USTC_CG_Camera* current_camera = get_free_camera(params, "Camera");

    auto& prev_camera = params.get_runtime_storage<PrevCamStatus&>();

    texture_info.isUAV = true;
    texture_info.format = nvrhi::Format::RGBA32_FLOAT;
    auto output = resource_allocator.create(texture_info);

    nvrhi::BindingLayoutDescVector binding_layout_desc_vec;
    std::string error_string;
    auto compute_shader = compile_shader(
        "main",
        nvrhi::ShaderType::Compute,
        "shaders/motion_vec.slang",
        binding_layout_desc_vec,
        error_string);
    MARK_DESTROY_NVRHI_RESOURCE(compute_shader);

    if (!error_string.empty()) {
        resource_allocator.destroy(output);
        throw std::runtime_error(error_string);
    }

    auto binding_layout = resource_allocator.create(binding_layout_desc_vec[0]);
    MARK_DESTROY_NVRHI_RESOURCE(binding_layout);

    ComputePipelineDesc pipeline_desc;
    pipeline_desc.CS = compute_shader;
    pipeline_desc.bindingLayouts = { binding_layout };
    auto compute_pipeline = resource_allocator.create(pipeline_desc);
    MARK_DESTROY_NVRHI_RESOURCE(compute_pipeline);

    auto command_list = resource_allocator.create(CommandListDesc{});
    MARK_DESTROY_NVRHI_RESOURCE(command_list);

    auto samplerDesc =
        nvrhi::SamplerDesc().setAllFilters(false).setAllAddressModes(
            nvrhi::SamplerAddressMode::Clamp);
    auto point_sampler = resource_allocator.create(samplerDesc);
    MARK_DESTROY_NVRHI_RESOURCE(point_sampler);

    auto cb_desc = BufferDesc()
                       .setIsConstantBuffer(true)
                       .setByteSize(sizeof(FrameConstants))
                       .setCpuAccess(nvrhi::CpuAccessMode::Write);

    auto frame_constants = resource_allocator.create(cb_desc);
    MARK_DESTROY_NVRHI_RESOURCE(frame_constants);

    BindingSetDesc binding_set_desc;
    binding_set_desc.bindings = {
        nvrhi::BindingSetItem::Texture_SRV(0, world_position),
        nvrhi::BindingSetItem::Texture_UAV(0, output),
        nvrhi::BindingSetItem::Sampler(0, point_sampler),
        nvrhi::BindingSetItem::ConstantBuffer(0, frame_constants)
    };
    auto binding_set =
        resource_allocator.create(binding_set_desc, binding_layout.Get());
    MARK_DESTROY_NVRHI_RESOURCE(binding_set);

    FrameConstants cpu_frame_constants;
    cpu_frame_constants.PrevProjViewMatrix = prev_camera.PrevProjViewMatrix;
    cpu_frame_constants.Resolution[0] = texture_info.width;
    cpu_frame_constants.Resolution[1] = texture_info.height;

    command_list->open();
    command_list->writeBuffer(
        frame_constants, &cpu_frame_constants, sizeof(FrameConstants));
    nvrhi::ComputeState compute_state;
    compute_state.pipeline = compute_pipeline;
    compute_state.addBindingSet(binding_set);
    command_list->setComputeState(compute_state);
    command_list->dispatch(
        div_ceil(texture_info.width, 8), div_ceil(texture_info.height, 8));
    command_list->close();

    resource_allocator.device->executeCommandList(command_list);

    params.set_output("Motion Vector", output);
    prev_camera.PrevProjViewMatrix =
        current_camera->viewMatrix * current_camera->projMatrix;
}

static void node_register()
{
    static NodeTypeInfo ntype;

    strcpy(ntype.ui_name, "Motion Vector");
    strcpy(ntype.id_name, "node_render_motion_vec");

    render_node_type_base(&ntype);
    ntype.node_execute = node_exec;
    ntype.declare = node_declare;
    nodeRegisterType(&ntype);
}

NOD_REGISTER_NODE(node_register)
}  // namespace USTC_CG::node_render_motion_vec