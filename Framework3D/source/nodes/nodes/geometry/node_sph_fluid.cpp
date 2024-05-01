#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <chrono>
#include <iostream>
#include <memory>

#include "GCore/Components/PointsComponent.h"
#include "Nodes/node.hpp"
#include "Nodes/node_declare.hpp"
#include "Nodes/node_register.h"
#include "geom_node_base.h"
#include "sph_fluid/wcsph.h"
#include "sph_fluid/iisph.h"
#include "sph_fluid/sph_base.h"
#include "utils/util_openmesh_bind.h"

namespace USTC_CG::node_sph_fluid {

// ------------------------- helper functions -------------------------------

pxr::VtArray<pxr::GfVec3f> eigen_to_usd_vertices(const Eigen::MatrixXd& V)
{
    pxr::VtArray<pxr::GfVec3f> vertices;
    for (int i = 0; i < V.rows(); i++) {
        vertices.push_back(pxr::GfVec3f(V(i, 0), V(i, 1), V(i, 2)));
    }
    return vertices;
}

// --------------------------------------------------------------------------
using namespace Eigen;

static void node_sph_fluid_declare(NodeDeclarationBuilder& b)
{

    b.add_input<decl::SPHFluidSocket>("SPH Class");

    // Simulation parameters 
    b.add_input<decl::Float3>("sim box min");
    b.add_input<decl::Float3>("sim box max");

    b.add_input<decl::Float3>("particle box min");
    b.add_input<decl::Float3>("particle box max");
    b.add_input<decl::Float3>("num particle per axis");

    b.add_input<decl::Float>("dt").default_val(0.01).min(0.0).max(0.5);
    b.add_input<decl::Float>("stiffness").default_val(1000).min(100).max(10000);
    b.add_input<decl::Float>("gravity").default_val(-9.8);

    // --------- HW Optional: if you implement sphere collision, please uncomment the following lines ------------
    //b.add_input<decl::Float>("collision penalty_k").default_val(10000).min(100).max(100000); 
    //b.add_input<decl::Float>("collision scale factor").default_val(1.1).min(1.0).max(2.0); 
    //b.add_input<decl::Float>("sphere radius").default_val(0.4).min(0.0).max(5.0);; 
    //b.add_input<decl::Float3>("sphere center");
    // -----------------------------------------------------------------------------------------------------------

    // Useful switches (0 or 1). You can add more if you like.
    b.add_input<decl::Int>("enable time profiling").default_val(0).min(0).max(1);
    b.add_input<decl::Int>("enable debug output").default_val(0).min(0).max(1);

    // Optional switches
    b.add_input<decl::Int>("enable IISPH").default_val(0).min(0).max(1);

    // Current time in node system 
    b.add_input<decl::Float>("time_code");

    // Output 
    b.add_output<decl::SPHFluidSocket>("SPH Class");
    b.add_output<decl::Geometry>("Points");
}

static void node_sph_fluid_exec(ExeParams params)
{
    auto time_code = params.get_input<float>("time_code");


    auto sph_base = params.get_input<std::shared_ptr<SPHBase>>("SPH Class");

    // ----------------------------- Load and check simulation box area ------------------------------------------------
    auto sim_box_min = params.get_input<pxr::GfVec3f>("sim box min"); 
    auto sim_box_max = params.get_input<pxr::GfVec3f>("sim box max"); 
    
    if (sim_box_max[0] <= sim_box_min[0] || sim_box_max[1] <= sim_box_min[1] || sim_box_max[2] <= sim_box_min[2]) {
		throw std::runtime_error("Invalid simulation box.");
	}
    // --------------------------- Load and check particle simpling box area -------------------------------------------
    auto particle_box_min = params.get_input<pxr::GfVec3f>("particle box min"); 
    auto particle_box_max = params.get_input<pxr::GfVec3f>("particle box max"); 
    auto num_particle_per_axis = params.get_input<pxr::GfVec3f>("num particle per axis"); 
    if (particle_box_max[0] <= particle_box_min[0] || particle_box_max[1] <= particle_box_min[1] || particle_box_max[2] <= particle_box_min[2]) {
		throw std::runtime_error("Invalid particle samping box.");
	}
    if (num_particle_per_axis[0] <= 0 || num_particle_per_axis[1] <= 0 ||
        num_particle_per_axis[2] <= 0) {
                throw std::runtime_error("Invalid number of particles per axis.");
    }
    //------------------------------------------------------------------------------------------------------------------

    if (time_code == 0) {  // If time = 0, reset and initialize the sph base class
            if (sph_base != nullptr)
				sph_base.reset();

            // First, sample particles in a box area 
            Vector3d box_min{sim_box_min[0], sim_box_min[1], sim_box_min[2]};
            Vector3d box_max{sim_box_max[0], sim_box_max[1], sim_box_max[2]};

            MatrixXd particle_pos = ParticleSystem::sample_particle_pos_in_a_box(
                box_min, box_max, 
                { static_cast<int>(num_particle_per_axis[0]), 
                  static_cast<int>(num_particle_per_axis[1]),
                  static_cast<int>(num_particle_per_axis[2]) });         

            bool enable_IISPH =  params.get_input<int>("enable IISPH") == 1 ? true : false;
            
            if (enable_IISPH) {
		    	sph_base = std::make_shared<IISPH>(particle_pos, box_min, box_max);
			}
			else {
				sph_base = std::make_shared<WCSPH>(particle_pos, box_min, box_max);
            }

            const float dt = params.get_input<float>("dt");
            sph_base->dt() = dt; 
            //sph_base->gravity()  = { 0, 0, params.get_input<float>("gravity") };

            if (enable_IISPH) {
			// --------- HW Optional: if you implement sphere collision, please uncomment the following lines -----------		
            
            // --------------------------------------------------------------------------------------------------------
            }
            else {
                const float k = params.get_input<float>("stiffness");
            }

    }
    else  // otherwise, step forward the simulation
    {
       sph_base->step(); 
    }

   // mesh->vertices = eigen_to_usd_vertices(mass_spring->getX());

    params.set_output("SPH Class", sph_base);

    auto geometry = GOperandBase();
    auto points_component = std::make_shared<PointsComponent>(&geometry);
    geometry.attach_component(points_component);

	auto vertices = sph_base->getX();
    points_component->vertices = eigen_to_usd_vertices(vertices);
    points_component->width = pxr::VtArray<float>(vertices.rows(), 0.1);

    params.set_output("Points", geometry);

}

static void node_register()
{
    static NodeTypeInfo ntype;

    strcpy(ntype.ui_name, "SPH Fluid");
    strcpy_s(ntype.id_name, "geom_sph_fluid");

    geo_node_type_base(&ntype);
    ntype.node_execute = node_sph_fluid_exec;
    ntype.declare = node_sph_fluid_declare;
    nodeRegisterType(&ntype);
}

NOD_REGISTER_NODE(node_register)
}  // namespace USTC_CG::node_sph_fluid