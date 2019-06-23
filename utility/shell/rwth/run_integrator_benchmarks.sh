#!/bin/bash

cd ../../../build/pars_benchmark_generator/

# Integrators 26GB
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_euler.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_modified_midpoint.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_runge_kutta_4.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_runge_kutta_cash_karp_54.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_runge_kutta_dormand_prince_5.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_runge_kutta_fehlberg_78.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_adams_bashforth_2.sh
sbatch benchmark_sc2_n8_p48_st16_i1024_lb1_adams_bashforth_moulton_2.sh

# Integrators 208GB
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_euler.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_modified_midpoint.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_runge_kutta_4.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_runge_kutta_cash_karp_54.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_runge_kutta_dormand_prince_5.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_runge_kutta_fehlberg_78.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_adams_bashforth_2.sh
sbatch benchmark_sc2_n32_p48_st32_i1024_lb1_adams_bashforth_moulton_2.sh

# Integrators 1.6TB
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_euler.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_modified_midpoint.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_runge_kutta_4.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_runge_kutta_cash_karp_54.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_runge_kutta_dormand_prince_5.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_runge_kutta_fehlberg_78.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_adams_bashforth_2.sh
sbatch benchmark_sc2_n128_p48_st64_i1024_lb1_adams_bashforth_moulton_2.sh