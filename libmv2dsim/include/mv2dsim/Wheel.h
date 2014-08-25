/*+-------------------------------------------------------------------------+
  |                       MultiVehicle 2D simulator (libmv2dsim)            |
  |                                                                         |
  | Copyright (C) 2014  Jose Luis Blanco Claraco (University of Almeria)    |
  | Distributed under GNU General Public License version 3                  |
  |   See <http://www.gnu.org/licenses/>                                    |
  +-------------------------------------------------------------------------+  */

#pragma once

#include <mv2dsim/basic_types.h>

#include <mrpt/poses/CPose2D.h>
#include <mrpt/utils/TColor.h>

namespace mv2dsim
{
	class DefaultFriction;
	class VehicleBase;
	class DynamicsDifferential;

	/** Common info for 2D wheels, for usage in derived classes.
		* Wheels are modeled as a mass with a rectangular shape.
		*/
	class Wheel
	{
		friend class DefaultFriction;
		friend class VehicleBase;
		friend class DynamicsDifferential;

	public:
		double x,y,yaw; //!< Location of the wheel wrt the chassis ref point [m,rad] (in local coords)
		double diameter,width; //!< Length(diameter) and width of the wheel rectangle [m]
		double mass; //!< [kg]
		double Iyy;  //!< Inertia (Computed automatically from geometry at constructor)
		mrpt::utils::TColor color; //!< Color for OpenGL rendering

		Wheel();
		void getAs3DObject(mrpt::opengl::CSetOfObjects &obj);
		void loadFromXML(const rapidxml::xml_node<char> *xml_node);

	protected:
		double phi, w; //!< Angular position and velocity of the wheel as it spins over its shaft (rad, rad/s)

	};

}
