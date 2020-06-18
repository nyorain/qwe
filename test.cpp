#include "serialize.hpp"
#include <cstdio>
#include <fstream>
#include <string>

struct Atmosphere {
	float bottom;
	float top;
	float sunAngularRadius;
	float minMuS;
	float groundAlbedo;

	struct {
		float g;
		float scaleHeight;
		struct {
			std::array<float, 3> rgb;
		} scattering;
	} mie;

	struct {
		float scaleHeight;
		struct {
			std::array<float, 3> rgb;
		} scattering;
	} rayleigh;

	struct {
		std::array<float, 3> rgb;
		struct {
			float start;
			float end;
			std::vector<float> values;
		} spectral;
	} solarIrradiance;
};

template<> struct Serializer<Atmosphere> : public PodSerializer<Atmosphere> {
	template<typename AtmosCV>
	static constexpr auto map(AtmosCV& atmos) {
		auto& si = atmos.solarIrradiance;
		return std::tuple{
			MapEntry{"bottom", atmos.bottom, true},
			MapEntry{"top", atmos.top, true},
			MapEntry{"sun_angular_radius", atmos.sunAngularRadius, true},
			MapEntry{"min_mu_s", atmos.sunAngularRadius, true},
			MapEntry{"ground_albedo", atmos.groundAlbedo, true},

			MapEntry{"mie.g", atmos.mie.g, true},
			MapEntry{"mie.scale_height", atmos.mie.scaleHeight, true},
			MapEntry{"mie.scattering.rgb", atmos.mie.scattering.rgb, true},

			MapEntry{"rayleigh.scale_height", atmos.rayleigh.scaleHeight, true},
			MapEntry{"rayleigh.scattering.rgb", atmos.rayleigh.scattering.rgb, true},

			MapEntry{"solar_irradiance.rgb", si.rgb, true},
			MapEntry{"solar_irradiance.spectral.start", si.spectral.start, true},
			MapEntry{"solar_irradiance.spectral.end", si.spectral.end, true},
			MapEntry{"solar_irradiance.spectral.values", si.spectral.values, true},
		};
	}
};

std::string readFile(std::string_view filename) {
	auto openmode = std::ios::ate;
	std::ifstream ifs(std::string{filename}, openmode);
	ifs.exceptions(std::ostream::failbit | std::ostream::badbit);

	auto size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::string buffer;
	buffer.resize(size);
	auto data = reinterpret_cast<char*>(buffer.data());
	ifs.read(data, size);

	return buffer;
}

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto file = readFile(argv[1]);
	file.c_str(); // make sure it's null terminated...
	Parser parser{file};

	auto pr = parse<Atmosphere>(parser);
	if(auto err = std::get_if<ErrorType>(&pr); err) {
		std::printf("error %d at %d:%d\n", (unsigned) *err,
			1 + parser.location.line, 1 + parser.location.col);
	} else {
		auto& t = std::get<Atmosphere>(pr);
		auto str = print(t);
		std::printf("%s\n", str.c_str());
	}
}
