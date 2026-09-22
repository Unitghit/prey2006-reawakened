// Opt-in CPU hitch diagnostics. Nested scopes overlap; do not sum their times.
#ifndef PREY_HITCH_TRACE_H
#define PREY_HITCH_TRACE_H
#include <chrono>
class idHitchScope {
	bool enabled;
	const char *category, *name;
	double minimum;
	std::chrono::steady_clock::time_point start;
public:
	idHitchScope( const char *category, const char *name, bool eligible = true, double minimum = 2.0 ) :
		enabled( eligible && cvarSystem->GetCVarBool( "com_hitchTrace" ) ), category(category), name(name), minimum(minimum) {
		if ( enabled ) start = std::chrono::steady_clock::now();
	}
	~idHitchScope() {
		if ( !enabled ) return;
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		if ( ms >= minimum ) common->Printf( "HITCH_OP wall=%u category=%s ms=%.3f asset=%s\n",
			sys->GetMilliseconds(), category, ms, name ? name : "" );
	}
};
#endif
