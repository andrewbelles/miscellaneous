#import astropy as ast
from astropy import units as u, coordinates as coord, time as time 
from astropy.constants import GM_earth, R_earth
from astropy.units import m, kg, km, s, deg

import numpy as np
#import matplotlib as plt
import argparse
from datetime import datetime, timezone 

omega_earth = 7.2921159e-5 * (1/s) 
j2_earth    = 1.0826359e-3

# Conversion of geographic coordinate to ECI coordinates
def geographic_to_eci(lat, lon, alt, time):
    geographic_coordinates = coord.EarthLocation.from_geodetic(lat=lat, lon=lon, height=alt)
    
    # Convert ITRS coordinates from geographic location 
    itrs_coordinates = coord.ITRS(
        x=geographic_coordinates.x,
        y=geographic_coordinates.y,
        z=geographic_coordinates.z,
        obstime=time
    )

    # Return the transformation from ITRS to ICRS
    return itrs_coordinates.transform_to(coord.GCRS(obstime=time))


# Class defn of cubesat satellite taking in magnetic field information  
class Cubesat:
    def __init__(self, lat, lon, alt, time, inclination=45*deg, raan=30*deg, true_anomaly=60*deg):
        # Convert all inputs to SI units
        alt_m = alt.to(m)
        self.coordinate_pos = geographic_to_eci(lat, lon, alt, time).cartesian.xyz.to(m)
        self.__mass = 16 * kg  # Correct unit syntax
        self.__altitude = alt_m

        # Correct angle assignments
        self.__i = np.radians(inclination)       # Inclination (radians)
        self.__omega = np.radians(raan)          # RAAN (radians)
        self.__theta = np.radians(true_anomaly)  # True anomaly (radians)

        # Calculate orbital velocity (SI units)
        r = R_earth + alt_m
        v_scalar = np.sqrt(GM_earth / r).to(m/s)
        
        # Velocity components (correct formula)
        v_x = v_scalar * (-np.sin(self.__theta) * np.cos(self.__omega) 
                          - np.cos(self.__theta) * np.sin(self.__omega) * np.cos(self.__i))
        v_y = v_scalar * (-np.sin(self.__theta) * np.sin(self.__omega) 
                          + np.cos(self.__theta) * np.cos(self.__omega) * np.cos(self.__i))
        v_z = v_scalar * np.cos(self.__theta) * np.sin(self.__i)
        
        self.__velocity = np.stack([v_x, v_y, v_z])
        self.__time     = time

    def exponential_density(self):
        h = self.__altitude.to(km)
        rho0 = 1.225 * kg/m**3
        H = 8.5 * km  # Match units with altitude
        return rho0 * np.exp(-(h - 500 * km)/H)

    def leo_acceleration(self, r, v):
        r_norm = np.linalg.norm(r)
        r_unit = (r / r_norm).to(u.dimensionless_unscaled).value  # Unit vector (dimensionless)
        z_unit = r_unit[2]  # Dimensionless z-component

        # Gravitational acceleration (correct formula)
        a_grav = -(GM_earth / r_norm**3) * r

        # Corrected J2 perturbation (units: m/s²)
        a_j2 = (3 * GM_earth * j2_earth * R_earth**2 / (2 * r_norm**5)) * (
            (5 * z_unit**2 - 1) * r - 2 * z_unit * r_norm * np.array([0, 0, 1])
        )

        # Drag acceleration (SI units)
        rho = self.exponential_density().to(kg/m**3)
        v_rel = v - np.cross(omega_earth * np.array([0, 0, 1]), r)
        a_m = 0.0075 * m**2
        a_drag = -0.5 * rho * 2.2 * a_m * np.linalg.norm(v_rel) * v_rel / self.__mass


        print(a_grav.unit)
        print(a_j2.unit)
        print(a_drag.unit)

        return (a_grav + a_j2 + a_drag).to(m/s**2)

    # Updates state vector by one timestep - RK4
    def runge_timestep(self, dt):

        r = self.coordinate_pos.to(m).value 
        v = self.__velocity.to(m/s).value
        state = np.concatenate([r.value, v.value]) * m/s
        
        def derivative(state):
            r = state[:3] * m 
            v = state[3:] * m/s

            a = self.leo_acceleration(r, v)
            print(a.unit)

            return np.concatenate([v.value, a.value]) * m/s

        k1 = derivative(state)
        k2 = derivative(state + 0.5 * dt * s * k1)
        k3 = derivative(state + 0.5 * dt * s* k2)
        k4 = derivative(state + dt* s * k3)

        state_new = state + (dt / 6.0) * (k1 + 2*k2 + 2*k3 + k4)

        self.coordinate_pos = coord.CartesianRepresentation(state_new[:3] * m)
        self.__velocity = state_new[3:] * m/s
        self.__time += dt * s

    def get_time(self):
        return self.__time



def main():
    parser = argparse.ArgumentParser()
    # Default position to above christmas valley
    parser.add_argument("--latitude", type=float, default=43.2378)
    parser.add_argument("--longitude", type=float, default=-120.6709)
    # True for infromational plots on "data"
    parser.add_argument("--plot", type=bool, default=False)
    args = parser.parse_args()

    lat = args.latitude * deg 
    lon = args.longitude * deg 

    # Initial Altitude is hardcoded to ionosphere
    alt = 500 * km 

    # Get current time
    sys_time    = datetime.now(timezone.utc)
    current_time = time.Time(sys_time)

    eci_coordinates = geographic_to_eci(lat, lon, alt, current_time);

    print(f"Earth Centered Inertial Position: {eci_coordinates.cartesian.xyz}")
    print(f"Magnitude {np.linalg.norm(eci_coordinates.cartesian.xyz)}")
    print(f"Time: {current_time.iso}")

    cubesat = Cubesat(lat, lon, alt, current_time, 45*deg, 30*deg, 60*deg)
    dt = 10 

    for _ in range(100):
        cubesat.runge_timestep(dt)
        print(f"Position: {cubesat.coordinate_pos.cartesian.xyz}")
        print(f"Time: {cubesat.get_time()}")


if __name__ == "__main__":
    main()
