/*+-------------------------------------------------------------------------+
  |                       MultiVehicle simulator (libmvsim)                 |
  |                                                                         |
  | Copyright (C) 2014-2023  Jose Luis Blanco Claraco                       |
  | Copyright (C) 2017  Borys Tymchenko (Odessa Polytechnic University)     |
  | Distributed under 3-clause BSD License                                  |
  |   See COPYING                                                           |
  +-------------------------------------------------------------------------+ */

#include <mrpt/core/lock_helper.h>
#include <mrpt/math/TPose2D.h>
#include <mrpt/opengl/CPolyhedron.h>
#include <mrpt/poses/CPose2D.h>
#include <mrpt/system/filesystem.h>
#include <mvsim/FrictionModels/DefaultFriction.h>  // For use as default model
#include <mvsim/FrictionModels/EllipseCurveMethod.h>
#include <mvsim/FrictionModels/FrictionBase.h>
#include <mvsim/Simulable.h>
#include <mvsim/VehicleBase.h>
#include <mvsim/VehicleDynamics/VehicleAckermann.h>
#include <mvsim/VehicleDynamics/VehicleAckermann_Drivetrain.h>
#include <mvsim/VehicleDynamics/VehicleDifferential.h>
#include <mvsim/Wheel.h>
#include <mvsim/World.h>

#include <cmath>
#include <map>
#include <rapidxml.hpp>
#include <string>

#include "JointXMLnode.h"
#include "XMLClassesRegistry.h"
#include "parse_utils.h"
#include "xml_utils.h"

using namespace mvsim;

// adapto los codigos ya existentes para crear el nuevo
EllipseCurveMethod::EllipseCurveMethod(
	VehicleBase& my_vehicle, const rapidxml::xml_node<char>* node)
	: FrictionBase(my_vehicle), CA_(8), Caf_(8.5), Cs_(7.5), ss_(0.1), Cafs_(0.5), Csaf_(0.5)
{
	// Sanity: we can tolerate node==nullptr (=> means use default params).
	if (node && 0 != strcmp(node->name(), "friction"))
		throw std::runtime_error("<friction>...</friction> XML node was expected!!");

	// Parse XML params:
	if (node) parse_xmlnode_children_as_param(*node, params_, world_->user_defined_variables());
}

// aqui ya cambia el codigo
namespace
{
// Heaviside function
double miH(double x, double x0)
{
	if (x > x0)
		return 1.0;
	else
		return 0.0;
}
// Saturation function
double miS(double x, double x0) { return x * miH(x0, std::abs(x)) + x0 * miH(std::abs(x), x0); }
}  // namespace

// See docs in base class.
mrpt::math::TVector2D EllipseCurveMethod::evaluate_friction(
	const FrictionBase::TFrictionInput& input) const
{
	// Rotate wheel velocity vector from veh. frame => wheel frame
	const mrpt::poses::CPose2D wRot(0, 0, input.wheel.yaw);

	// Velocity of the wheel cog in the frame of the wheel itself:
	const mrpt::math::TVector2D vel_w = wRot.inverseComposePoint(input.wheelCogLocalVel);

	const mrpt::math::TTwist2D& vel = myVehicle_.getVelocityLocal();  // ¿Está bien?
	const double m = myVehicle_.getChassisMass();  // masa del conjunto
	const double afs = 5.0 * M_PI / 180.0;
	const double CA = CA_;
	const double gravity = myVehicle_.parent()->get_gravity();
	const double R = 0.5 * input.wheel.diameter;  // Wheel radius
	const double w = vel.omega;
	const double delta = input.wheel.getPhi();	// angulo de la rueda¿Está bien?

	// obtener posiciones y distancias de ejes
	mrpt::math::TPoint2D Center_of_mass =
		myVehicle_.getChassisCenterOfMass();  // ¿esta bien este codigo?// ¿esta bien este codigo?
	const double h = 0.40;	// altura del centro de gravedad provisional
	const size_t nW =
		myVehicle_.getNumWheels();	// esta linea ya existe en VehicleBase.ccp linea 568
	std::vector<mrpt::math::TVector2D> pos(nW);

	for (size_t i = 0; i < nW;
		 i++)  
	{
		const Wheel& wpos = myVehicle_.getWheelInfo(i);
		pos[i].x = wpos.x - Center_of_mass.x;
		pos[i].y = wpos.y - Center_of_mass.y;
	}

	const double a1 = std::abs(pos[3].x),
				 a2 = std::abs(pos[0].x);  // distancia centro de gravedad a ejes
	const double l = a1 + a2;  // distancia entre ejes
	const double Axf = std::abs(pos[2].y) + std::abs(pos[3].y),
				 Axr = std::abs(pos[0].y) + std::abs(pos[1].y);	 // longitud ejes
	ASSERT_(Axf > 0);
	ASSERT_(Axr > 0);

	const mrpt::math::TPoint3D_<double> linAccLocal =
		myVehicle_.getLinearAcceleration();	 // ¿Está bien?

	// 1) Vertical forces (decoupled sub-problem)
	// --------------------------------------------
	//// crear un if para cada rueda
	// Wheels: [0]:rear-left, [1]:rear-right, [2]: front-left, [3]: front-right

	// esto da error, mi intención es que detecte para que rueda es el calculo

	if (wheel == 3)
	{
		double Fz = (m / (l * Axf * gravity)) * (a2 * gravity - h * (linAccLocal.x - w * vel.vy)) *
					(std::abs(pos[1].y) * gravity - h * (linAccLocal.y + w * vel.vx));
	}
	else if (wheel == 2)
	{
		double Fz = (m / (l * Axf * gravity)) * (a2 * gravity - h * (linAccLocal.x - w * vel.vy)) *
					(std::abs(pos[0].y) * gravity + h * (linAccLocal.y + w * vel.vx));
	}
	else if (wheel == 1)
	{
		double Fz = (m / (l * Axr * gravity)) * (a1 * gravity + h * (linAccLocal.x - w * vel.vy)) *
					(std::abs(pos[3].y) * gravity - h * (linAccLocal.y + w * vel.vx));
	}
	else if (wheel == 0)
	{
		double Fz = (m / (l * Axr * gravity)) * (a1 * gravity + h * (linAccLocal.x - w * vel.vy)) *
					(std::abs(pos[2].y) * gravity + h * (linAccLocal.y + w * vel.vx));
	}
	else
	{
		throw std::runtime_error(
			"Invalid wheel index");	 // Revisar, esta linea me la ha generado copilot
	}

	double max_friction = Fz;

	// 2) Wheels velocity at Tire SR (decoupled sub-problem)
	// -------------------------------------------------
	// duda de cambiar el codigo o no) VehicleBase.cpp line 575 calcula esto pero distinto
	double vxT = (vel.vx - w * pos.y) * cos(delta) + (vel.vy + w * pos.x) * sin(delta);

	// 3) Longitudinal slip (decoupled sub-problem)
	// -------------------------------------------------

	// w= velocidad angular
	double s = (R * input.wheel.getW() - vxT) /
			   (R * input.wheel.getW() * miH(R * input.wheel.getW(), vxT) +
				vxT * miH(vxT, R * input.wheel.getW()));

	if (std::isnan(s)) s = 0;

	// 4) Sideslip angle (decoupled sub-problem)
	// -------------------------------------------------

	double af = atan2((vel.vy + pos.x * w), (vel.vx - pos.y * w)) - delta;

	// 5) Longitudinal friction (decoupled sub-problem)
	// -------------------------------------------------
	double wheel_long_friction =
		max_friction * Cs_ * miS(s, ss_) * sqrt(1 - Csaf_ * pow((miS(af, afs) / afs), 2));

	// 6) Lateral friction (decoupled sub-problem)
	// --------------------------------------------
	double wheel_lat_friction =
		-max_friction * Caf_ * miS(af, afs) * sqrt(1 - Cafs_ * pow((miS(s, ss_) / ss_), 2));

	// Recalc wheel ang. velocity impulse with this reduced force:
	const double I_yy = input.wheel.Iyy;
	const double actual_wheel_alpha = (input.motorTorque / I_yy) - R * wheel_long_friction / I_yy;

	input.wheel.setW(input.wheel.getW() + actual_wheel_alpha * input.context.dt);

	// Resultant force: In local (x,y) coordinates (Newtons) wrt the Wheel
	// -----------------------------------------------------------------------
	const mrpt::math::TPoint2D result_force_wrt_wheel(wheel_long_friction, wheel_lat_friction);

	// Rotate to put: Wheel frame ==> vehicle local framework:
	mrpt::math::TVector2D res;
	wRot.composePoint(result_force_wrt_wheel, res);
	return res;
}
