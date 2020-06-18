#include "serialize.hpp"
#include <cstdio>
#include <fstream>
#include <string>

struct Test {
	float a;
	int b;
	float c;
	std::string d;
	std::vector<Test> e;

	struct {
		float a;
		float b;
	} f;
};

template<> struct Serializer<Test> : public PodSerializer<Test> {
	static constexpr auto map = PodSerializer<Test>::entryMap(
		PodEntry{"a", &Test::a, true},
		PodEntry{"b", &Test::b},
		PodEntry{"c", &Test::c},
		PodEntry{"d", &Test::d, true},
		PodEntry{"e", &Test::e},

		PodEntry{"f", &Test::f, false, std::tuple{
			PodEntry{"a", &decltype(Test::f)::a},
			PodEntry{"b", &decltype(Test::f)::b},
		}}
	);
};

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

template<> struct Serializer<Atmosphere> : public PodSerializer2<Atmosphere> {
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


	/*
	using T = Atmosphere;
	using Mie = decltype(T::mie);
	using Rayleigh = decltype(T::rayleigh);
	using SI = decltype(T::solarIrradiance);

	static constexpr auto map = PodSerializer<Test>::entryMap(
		PodEntry{"bottom", &T::bottom, true},
		PodEntry{"top", &T::bottom, true},
		PodEntry{"sun_angular_radius", &T::sunAngularRadius, true},
		PodEntry{"min_mu_s", &T::minMuS, true},
		PodEntry{"ground_albedo", &T::groundAlbedo, true},
		PodEntry{"mie", &T::mie, true, std::tuple{
			PodEntry{"g", &Mie::g, true},
			PodEntry{"scale_height", &Mie::scaleHeight, true},
			PodEntry{"scattering", &Mie::scattering, true, std::tuple{
				PodEntry{"rgb", &decltype(Mie::scattering)::rgb, true}
			}},
		}},
		PodEntry{"rayleigh", &T::rayleigh, true, std::tuple{
			PodEntry{"scale_height", &Rayleigh::scaleHeight, true},
			PodEntry{"scattering", &Rayleigh::scattering, true, std::tuple{
				PodEntry{"rgb", &decltype(Rayleigh::scattering)::rgb, true}
			}},
		}},
		PodEntry{"solar_irradiance", &T::solarIrradiance, true, std::tuple{
			PodEntry{"rgb", &SI::rgb, true},
			PodEntry{"spectral", &SI::spectral, true, std::tuple{
				PodEntry{"start", &decltype(SI::spectral)::start, true},
				PodEntry{"end", &decltype(SI::spectral)::end, true},
				PodEntry{"values", &decltype(SI::spectral)::values, true},
			}},
		}}
	);
	*/
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

void print(const Test& t, std::string indent = "") {
	std::printf("%sa: %f, b: %d, c: %f, d: '%s', f.a: %f, f.b: %f\n",
		indent.c_str(), t.a, t.b, t.c, t.d.c_str(), t.f.a, t.f.b);

	indent += "\t";
	for(auto& v : t.e) {
		print(v, indent);
	}
}

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto file = readFile(argv[1]);
	file.c_str(); // make sure it's null terminated...
	Parser parser{file};

	/*
	auto pr = parse<Test>(parser);
	if(auto err = std::get_if<ErrorType>(&pr); err) {
		std::printf("error %d at %d:%d\n", (unsigned) *err,
			1 + parser.location.line, 1 + parser.location.col);
	} else {
		auto& t = std::get<Test>(pr);
		print(t);
	}
	*/

	auto pr = parse<Atmosphere>(parser);
	if(auto err = std::get_if<ErrorType>(&pr); err) {
		std::printf("error %d at %d:%d\n", (unsigned) *err,
			1 + parser.location.line, 1 + parser.location.col);
	} else {
		auto& t = std::get<Atmosphere>(pr);
		auto str = print(t);
		std::printf("%s\n", str.c_str());

		// std::printf("solarIrradiance.spectral.start: %f\n",
		// 	t.solarIrradiance.spectral.start);
	}
}
