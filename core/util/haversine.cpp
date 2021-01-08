#include "haversine.hpp"
#include <math.h>

static constexpr double EARTHRADIUS = 6371; // In km

static constexpr double pi()
{
	return std::atan(1)*4;
}

static double deg2rad(double degrees)
{
	return degrees * (pi() / 180.0);
}

double haversine_distance(double lon1, double lat1, double lon2, double lat2)
{
	double d_lat = deg2rad(lat2 - lat1);
	double d_lon = deg2rad(lon2 - lon1);
	double a = (std::sin(d_lat/2) * std::sin(d_lat/2)) +
               (std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) * std::sin(d_lon/2) * std::sin(d_lon/2));
	double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
    double d = EARTHRADIUS * c; // Distance in km
    return d;
}
