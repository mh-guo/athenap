//========================================================================================
// Athena++ astrophysical MHD code
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code contributors
// Licensed under the 3-clause BSD License, see LICENSE file for details
//========================================================================================
//! \file cool_turb.cpp
//! \brief Problem generator for turbulence driver with cooling

// C headers

// C++ headers
#include <cmath>
#include <ctime>
#include <sstream>
#include <stdexcept>

// Athena++ headers
#include "../athena.hpp"
#include "../athena_arrays.hpp"
#include "../coordinates/coordinates.hpp"
#include "../eos/eos.hpp"
#include "../fft/athena_fft.hpp"
#include "../field/field.hpp"
#include "../globals.hpp"
#include "../hydro/hydro.hpp"
#include "../mesh/mesh.hpp"
#include "../parameter_input.hpp"
#include "../utils/utils.hpp"

#ifdef OPENMP_PARALLEL
#include <omp.h>
#endif

Real TempToLambda(Real log10T){
  Real log10Lambda = 0.0;
  if (log10T<4.0) {
    log10Lambda = -24.0;
  } else if (log10T>=4.0 && log10T<5.0) {
    log10Lambda = -24.0 + (-20.5+24.0) * (log10T-4.0);
  } else if (log10T>=5.0 && log10T<7.5) {
    log10Lambda = -20.5 + (-22.5+20.5) * (log10T-5.0) / (7.5-5.0);
  } else if (log10T>=7.5 && log10T<9.0) {
    log10Lambda = -22.5 + (-22.0+22.5) * (log10T-7.5) / (9.0-7.5);
  } else {
    log10Lambda = -22.0 + (log10T-9.0) / 3.0;
  }
  return log10Lambda;
}

//========================================================================================
//! \fn void Mesh::InitUserMeshData(ParameterInput *pin)
//! \brief
//========================================================================================
void CoolingSource(MeshBlock *pmb, const Real time, const Real dt,
             const AthenaArray<Real> &prim, const AthenaArray<Real> &prim_scalar,
             const AthenaArray<Real> &bcc, AthenaArray<Real> &cons,
             AthenaArray<Real> &cons_scalar) {
  Real g = pmb->peos->GetGamma();
  Real temp_goal = 0.1;
  Real tau = 0.01;
  Real mu = 0.62; //An appropriate value for high-T Z_sun plasma
  Real kB = 1.3807e-16; //Boltzmann constant in cgs unit
  Real amu = 1.660539e-24; // Atomic Mass Unit in cgs unit
  // = pin->GetReal("cooling","tau");
  for (int k = pmb->ks; k <= pmb->ke; ++k) {
    for (int j = pmb->js; j <= pmb->je; ++j) {
      for (int i = pmb->is; i <= pmb->ie; ++i) {
        Real temp = prim(IPR,k,j,i) / prim(IDN,k,j,i) * mu * amu / kB;
        Real log10T = std::log10(temp);
        Real log10Lambda = TempToLambda(log10T);
        Real Lambda = std::pow((Real)10.0,log10Lambda);
        
        if (temp > temp_goal) {
          //cons(IEN,k,j,i) -= dt / tau * prim(IDN,k,j,i) * (temp - temp_goal) / (g - 1.0);
          cons(IEN,k,j,i) -= dt * prim(IDN,k,j,i) * prim(IDN,k,j,i) / amu / amu * Lambda;
        }
      }
    }
  }
  return;
}


//========================================================================================
//! \fn void Mesh::InitUserMeshData(ParameterInput *pin)
//  \brief
//========================================================================================
void Mesh::InitUserMeshData(ParameterInput *pin) {
  if (SELF_GRAVITY_ENABLED) {
    Real four_pi_G = pin->GetReal("problem","four_pi_G");
    Real eps = pin->GetOrAddReal("problem","grav_eps", 0.0);
    SetFourPiG(four_pi_G);
    SetGravityThreshold(eps);
  }

  // turb_flag is initialzed in the Mesh constructor to 0 by default;
  // turb_flag = 1 for decaying turbulence
  // turb_flag = 2 for impulsively driven turbulence
  // turb_flag = 3 for continuously driven turbulence
  turb_flag = pin->GetInteger("problem","turb_flag");
  if (turb_flag != 0) {
#ifndef FFT
    std::stringstream msg;
    msg << "### FATAL ERROR in TurbulenceDriver::TurbulenceDriver" << std::endl
        << "non zero Turbulence flag is set without FFT!" << std::endl;
    ATHENA_ERROR(msg);
    return;
#endif
  }

  EnrollUserExplicitSourceFunction(CoolingSource);

  return;
}

//========================================================================================
//! \fn void MeshBlock::ProblemGenerator(ParameterInput *pin)
//  \brief
//========================================================================================

void MeshBlock::ProblemGenerator(ParameterInput *pin) {
  Real rho = pin->GetReal("problem","rho");
  Real T = pin->GetReal("problem","T");
  Real gamma = pin->GetReal("hydro","gamma");
  Real gm1 = gamma - 1.0;
  Real mu = 0.62; //An appropriate value for high-T Z_sun plasma
  Real kB = 1.3807e-16; //Boltzmann constant in cgs unit
  Real amu = 1.660539e-24; // Atomic Mass Unit in cgs unit
  for (int k=ks; k<=ke; k++) {
    for (int j=js; j<=je; j++) {
      for (int i=is; i<=ie; i++) {
        phydro->u(IDN,k,j,i) = rho;

        phydro->u(IM1,k,j,i) = 0.0;
        phydro->u(IM2,k,j,i) = 0.0;
        phydro->u(IM3,k,j,i) = 0.0;

        if (NON_BAROTROPIC_EOS) {
          //phydro->u(IEN,k,j,i) = rho*T/gm1; // in unit of kB
          phydro->u(IEN,k,j,i) = rho*kB*T/gm1/mu/amu; // in cgs unit
        }
      }
    }
  }
}


//========================================================================================
//! \fn void Mesh::UserWorkAfterLoop(ParameterInput *pin)
//  \brief
//========================================================================================

void Mesh::UserWorkAfterLoop(ParameterInput *pin) {
}
