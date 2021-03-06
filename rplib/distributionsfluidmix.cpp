
#include "rplib/fluid.h"

#include "rplib/fluidmix.h"
#include "rplib/distributionsfluidmix.h"

#include "rplib/distributionwithtrend.h"
#include "rplib/demmodelling.h"

#include "src/definitions.h"

#include "nrlib/random/distribution.hpp"

#include <cassert>

DistributionsFluidMix::DistributionsFluidMix(const std::vector<double>                    & alpha,
                                             std::vector< DistributionsFluid * >          & distr_fluid,
                                             std::vector< DistributionWithTrend * >       & distr_vol_frac,
                                             DEMTools::MixMethod                            mix_method)
: DistributionsFluid(),
  distr_fluid_(distr_fluid.size(), NULL),
  distr_vol_frac_(distr_vol_frac.size(), NULL),
  mix_method_(mix_method)
{
  assert(distr_fluid.size() == distr_vol_frac.size());

  for (size_t i = 0; i < distr_fluid.size(); ++i)
    distr_fluid_[i] = distr_fluid[i]->Clone();

  for (size_t i = 0; i < distr_vol_frac.size(); ++i) {
    if(distr_vol_frac[i] != NULL) {
      if(distr_vol_frac[i]->GetIsShared() == false)
        distr_vol_frac_[i] = distr_vol_frac[i]->Clone();
      else
        distr_vol_frac_[i] = distr_vol_frac[i];
    }

  }

  alpha_ = alpha;
}

DistributionsFluidMix::DistributionsFluidMix(const DistributionsFluidMix & dist)
: DistributionsFluid(dist),
  mix_method_(dist.mix_method_)
{
  size_t fluid_size = dist.distr_fluid_.size();

  distr_fluid_.resize(fluid_size);
  for(size_t i=0; i<fluid_size; i++)
    distr_fluid_[i] = dist.distr_fluid_[i]->Clone();

  distr_vol_frac_.resize(fluid_size, NULL);
  for(size_t i=0; i<fluid_size; i++) {
    if(dist.distr_vol_frac_[i] != NULL) {
      if(dist.distr_vol_frac_[i]->GetIsShared() == false)
        distr_vol_frac_[i] = dist.distr_vol_frac_[i]->Clone();
      else
        distr_vol_frac_[i] = dist.distr_vol_frac_[i];
    }
  }

  alpha_               = dist.alpha_;
}

DistributionsFluidMix::~DistributionsFluidMix()
{
  for(size_t i=0; i<distr_fluid_.size(); i++)
    delete distr_fluid_[i];

  for(size_t i=0; i<distr_vol_frac_.size(); i++) {
    if(distr_vol_frac_[i] != NULL) {
      if(distr_vol_frac_[i]->GetIsShared() == false)
        delete distr_vol_frac_[i];
    }
  }
}

DistributionsFluid *
DistributionsFluidMix::Clone() const
{
  return new DistributionsFluidMix(*this);
}

Fluid *
DistributionsFluidMix::GenerateSample(const std::vector<double> & trend_params)
{
  size_t n_fluids = distr_fluid_.size();

  std::vector<double> u(n_fluids, RMISSING);
  for(size_t i=0; i<n_fluids; i++) {
    if(distr_vol_frac_[i] != NULL)
      u[i] = NRLib::Random::Unif01();
  }

  std::vector<Fluid*> fluid_samples(n_fluids);

  for(size_t i = 0; i < n_fluids; ++i)
    fluid_samples[i] = distr_fluid_[i]->GenerateSample(trend_params);

  Fluid * fluid_mixed = GetSample(u, trend_params, fluid_samples);

  // Deep copy taken by constructor of FluidMixed, hence delete fluid here:
  for(size_t i = 0; i < n_fluids; ++i)
    delete fluid_samples[i];

  return fluid_mixed;
}

Fluid *
DistributionsFluidMix::GetSample(const std::vector<double>  & u,
                                 const std::vector<double>  & trend_params,
                                 const std::vector<Fluid *> & fluid_samples)
{
  size_t n_fluids = fluid_samples.size();

  std::vector<double> volume_fraction(n_fluids, 0.0);

  size_t missing_index = n_fluids;

  for(size_t i = 0; i < n_fluids; ++i) {
    if(u[i] != RMISSING)
      volume_fraction[i] = distr_vol_frac_[i]->GetQuantileValue(u[i], trend_params[0], trend_params[1]);
    else
      missing_index = i;
  }

  if (missing_index != n_fluids) {
    double sum = 0.0;

    for (size_t i = 0; i < volume_fraction.size(); ++i)
      sum += volume_fraction[i];

    volume_fraction[missing_index] = 1.0 - sum;
  }

  Fluid * fluid_mixed = new FluidMix(fluid_samples, volume_fraction, u, mix_method_);

  return fluid_mixed;
}

bool
DistributionsFluidMix::HasDistribution() const
{
  bool has_distribution = false;

  size_t n_fluids = distr_fluid_.size();

  for(size_t i=0; i<n_fluids; i++) {

    if(distr_fluid_[i]->HasDistribution() == true)
      has_distribution = true;

    else if(distr_vol_frac_[i] != NULL && distr_vol_frac_[i]->GetIsDistribution() == true)
      has_distribution = true;
  }

  return has_distribution;
}

std::vector<bool>
DistributionsFluidMix::HasTrend() const
{
  std::vector<bool> has_trend(2, false);

  size_t n_fluids = distr_fluid_.size();

  for(size_t i=0; i<n_fluids; i++) {
    std::vector<bool> fluid_trend  = distr_fluid_[i]->HasTrend();

    std::vector<bool> volume_trend(2, false);

    if(distr_vol_frac_[i] != NULL)
       volume_trend = distr_vol_frac_[i]->GetUseTrendCube();

    for(int j=0; j<2; j++) {

      if(fluid_trend[j] == true)
        has_trend[j] = true;

      else if(volume_trend[j] == true)
        has_trend[j] = true;
    }
  }

  return has_trend;
}

Fluid *
DistributionsFluidMix::UpdateSample(double                      corr_param,
                                    bool                        param_is_time,
                                    const std::vector<double> & trend,
                                    const Fluid               * sample)
{
  std::vector<double> u = sample->GetU();
  DEMTools::UpdateU(u, corr_param, param_is_time, alpha_);

  assert(typeid(*sample) == typeid(FluidMix));
  const FluidMix * core_sample = dynamic_cast<const FluidMix *>(sample);

  std::vector<Fluid *> updated_sub_fluids(distr_fluid_.size());

  for(size_t i = 0; i<distr_fluid_.size(); i++) {
    updated_sub_fluids[i] = distr_fluid_[i]->UpdateSample(corr_param,
                                                          param_is_time,
                                                          trend,
                                                          core_sample->GetSubFluid(i));
  }
  Fluid * updated_sample = GetSample(u, trend, updated_sub_fluids);

  for(size_t i = 0; i<distr_fluid_.size(); i++)
    delete updated_sub_fluids[i];

  return updated_sample;

}

