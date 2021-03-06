#include <pa/stages/ftle_map_generator.hpp>

#include <Eigen/Dense>
#include <tbb/tbb.h>

namespace pa
{
ftle_map_generator::ftle_map_generator(partitioner* partitioner, mode mode) : partitioner_(partitioner), mode_(mode)
{

}

void                          ftle_map_generator::set_flow_map(vector_field* flow_map   )
{
  flow_map_ = flow_map;
}
void                          ftle_map_generator::set_time    (scalar        time       )
{
  time_ = time;
}

std::unique_ptr<scalar_field> ftle_map_generator::generate    ()
{
  auto ftle_map = std::make_unique<scalar_field>();
  ftle_map->data.resize(boost::extents
   [flow_map_->data.shape()[0]]
   [flow_map_->data.shape()[1]]
   [flow_map_->data.shape()[2]]);
  ftle_map->offset  = flow_map_->offset ;
  ftle_map->size    = flow_map_->size   ;
  ftle_map->spacing = flow_map_->spacing;

  auto gradient = flow_map_->gradient();
  tbb::parallel_for(tbb::blocked_range3d<std::size_t>(0, gradient->data.shape()[0], 0, gradient->data.shape()[1], 0, gradient->data.shape()[2]), 
    [&] (const tbb::blocked_range3d<std::size_t>& index) {
    for (auto x = index.pages().begin(), x_end = index.pages().end(); x < x_end; ++x) {
    for (auto y = index.rows ().begin(), y_end = index.rows ().end(); y < y_end; ++y) {
    for (auto z = index.cols ().begin(), z_end = index.cols ().end(); z < z_end; ++z) {
      // Compute the spectral norm (Left Cauchy-Green tensor).
      matrix3 spectral_norm = gradient->data[x][y][z].transpose().eval() * gradient->data[x][y][z];

      // Compute eigenvalues and eigenvectors.
      Eigen::SelfAdjointEigenSolver<matrix3> solver(spectral_norm);
      auto eigenvalues  = solver.eigenvalues ();
      auto eigenvectors = solver.eigenvectors();

      // Compute FTLE.
      pa::scalar value;
      if (mode_ == mode::regular)
        value = std::log(std::sqrt(eigenvalues.maxCoeff())) / std::abs(time_);
      else if (mode_ == mode::fractional_anisotropy)
        value = std::sqrt(0.5) * 
                std::sqrt(std::pow(eigenvalues[0] - eigenvalues[1], 2) + std::pow(eigenvalues[1] - eigenvalues[2], 2) + std::pow(eigenvalues[2] - eigenvalues[0], 2)) /
                std::sqrt(std::pow(eigenvalues[0], 2) + std::pow(eigenvalues[1], 2) + std::pow(eigenvalues[2], 2));

      ftle_map->data[x][y][z] = value;
  }}}});

  return ftle_map;
}
}