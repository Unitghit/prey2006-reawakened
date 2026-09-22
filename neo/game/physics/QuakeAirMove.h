// Quake 1 SV_AirAccelerate rule, adapted to Prey's gravity-relative wishdir.
// https://github.com/id-Software/Quake/blob/master/WinQuake/sv_user.c
#ifndef PREY_QUAKE_AIR_MOVE_H
#define PREY_QUAKE_AIR_MOVE_H
inline float PreyQuakeAirAcceleration( float wishSpeed, float projectedSpeed, float seconds ) {
    if ( wishSpeed <= 0.0f || seconds <= 0.0f ) return 0.0f;
    const float cappedWish = wishSpeed < 30.0f ? wishSpeed : 30.0f;
    const float remaining = cappedWish - projectedSpeed;
    if ( remaining <= 0.0f ) return 0.0f;
    const float acceleration = 10.0f * wishSpeed * seconds;
    return acceleration < remaining ? acceleration : remaining;
}
#endif
