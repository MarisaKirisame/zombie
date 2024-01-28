#pragma once

#include "microwave_anf.hpp"
#include "microwave_cps.hpp"
namespace ZombieInternal {

template<const ZombieConfig &cfg>
using MicroWave = ANF::MicroWave<cfg>;

template<const ZombieConfig &cfg>
using MicroWavePtr = ANF::MicroWavePtr<cfg>;

} // end of namespace ZombieInternal
