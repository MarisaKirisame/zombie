#pragma once

#include "microwave_anf.hpp"
#include "microwave_cps.hpp"
namespace ZombieInternal {

template<const ZombieConfig &cfg>
using MicroWave = std::conditional_t<cfg.use_cps, CPS::MicroWave<cfg>, ANF::MicroWave<cfg>>;

template<const ZombieConfig &cfg>
using MicroWavePtr = std::conditional_t<cfg.use_cps, CPS::MicroWavePtr<cfg>, ANF::MicroWavePtr<cfg>>;

} // end of namespace ZombieInternal
