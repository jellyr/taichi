/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2016 Yuanming Hu <yuanmhu@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#pragma once

#include <memory>
#include <vector>
#include <memory.h>
#include <string>
#include <functional>

#include <taichi/visualization/image_buffer.h>
#include <taichi/common/meta.h>
#include <taichi/dynamics/simulation3d.h>
#include <taichi/math/array_3d.h>
#include <taichi/math/qr_svd.h>
#include <taichi/math/levelset_3d.h>
#include <taichi/math/dynamic_levelset_3d.h>
#include <taichi/system/threading.h>

#include "mpm3_scheduler.h"
#include "mpm3_particle.h"

TC_NAMESPACE_BEGIN

class MPM3D : public Simulation3D {
protected:
    typedef Vector3 Vector;
    typedef Matrix3 Matrix;
    typedef Region3D Region;
public:
    static const int D = 3;

public:
    std::vector<MPM3Particle *> particles; // for efficiency
    Array3D<Vector> grid_velocity;
    Array3D<Vector> grid_velocity_backup;
    Array3D<Spinlock> grid_locks;
    Array3D<real> grid_mass;
    Vector3i res;
    int max_dim;
    Vector gravity;
    bool apic;

    bool async;
    real affine_damping;
    real base_delta_t;
    real maximum_delta_t;
    real cfl;
    real strength_dt_mul;
    real request_t = 0.0f;
    int64 current_t_int = 0;
    int64 original_t_int_increment;
    int64 t_int_increment;
    int64 old_t_int;
    MPM3Scheduler scheduler;

    Region get_bounded_rasterization_region(Vector p) {
        assert_info(is_normal(p.x) && is_normal(p.y) && is_normal(p.z),
                    std::string("Abnormal p: ") + std::to_string(p.x)
                    + ", " + std::to_string(p.y) + ", " + std::to_string(p.z));
        int x = int(p.x);
        int y = int(p.y);
        int z = int(p.z);
        /*
        int x_min = max(0, x - 1);
        int x_max = min(res[0], x + 3);
        int y_min = max(0, y - 1);
        int y_max = min(res[1], y + 3);
        int z_min = max(0, z - 1);
        int z_max = min(res[2], z + 3);
        */
        int x_min = std::max(0, std::min(res[0], x - 1));
        int x_max = std::max(0, std::min(res[0], x + 3));
        int y_min = std::max(0, std::min(res[1], y - 1));
        int y_max = std::max(0, std::min(res[1], y + 3));
        int z_min = std::max(0, std::min(res[2], z - 1));
        int z_max = std::max(0, std::min(res[2], z + 3));
        return Region(x_min, x_max, y_min, y_max, z_min, z_max);
    }

    bool test() const override;

    void estimate_volume() {}

    void rasterize();

    void resample();

    void grid_backup_velocity() {
        grid_velocity_backup = grid_velocity;
    }

    void apply_deformation_force(float delta_t);

    void grid_apply_boundary_conditions(const DynamicLevelSet3D &levelset, real t);

    void grid_apply_external_force(Vector acc, real delta_t) {
        for (auto &ind : grid_mass.get_region()) {
            if (grid_mass[ind] > 0) // Do not use EPS here!!
                grid_velocity[ind] += delta_t * acc;
        }
    }

    void particle_collision_resolution(real t);

    void substep();

    template <typename T>
    void parallel_for_each_particle(const T &target) {
        ThreadedTaskManager::run((int)particles.size(), num_threads, [&](int i) {
            target(*particles[i]);
        });
    }

    template <typename T>
    void parallel_for_each_active_particle(const T &target) {
        ThreadedTaskManager::run((int)scheduler.get_active_particles().size(), num_threads, [&](int i) {
            target(*scheduler.get_active_particles()[i]);
        });
    }

public:

    MPM3D() {}

    virtual void initialize(const Config &config) override;

    virtual void add_particles(const Config &config) override;

    virtual void step(real dt) override {
        if (dt < 0) {
            substep();
            request_t = current_t;
        } else {
            request_t += dt;
            while (current_t + base_delta_t < request_t) {
                substep();
            }
            P(t_int_increment * base_delta_t);
        }
    }

    std::vector<RenderParticle> get_render_particles() const override;

    ~MPM3D() {
        for (auto &p : particles) {
            delete p;
        }
    }
};

TC_NAMESPACE_END

