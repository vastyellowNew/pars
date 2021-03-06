#ifndef PA_STAGES_FLOW_MAP_GENERATOR_HPP
#define PA_STAGES_FLOW_MAP_GENERATOR_HPP

#include <memory>

#include <pa/math/vector_field.hpp>
#include <pa/stages/particle_advector.hpp>
#include <pa/export.hpp>

namespace pa
{
class PA_EXPORT flow_map_generator : public particle_advector
{
public:
  explicit flow_map_generator  (partitioner* partitioner);
  flow_map_generator           (const flow_map_generator&  that) = delete ;
  flow_map_generator           (      flow_map_generator&& temp) = delete ;
  virtual ~flow_map_generator  ()                                = default;
  flow_map_generator& operator=(const flow_map_generator&  that) = delete ;
  flow_map_generator& operator=(      flow_map_generator&& temp) = delete ;

  std::unique_ptr<vector_field> generate  (const std::size_t iterations, const scalar resolution_scale = 1);

  // Generate sub-methods for separate benchmarking.
  void                          allocate  (const scalar      resolution_scale,                                               std::unique_ptr<vector_field>& flow_map);
  void                          initialize(const std::size_t iterations      ,       std::vector<particle>& particles, const std::unique_ptr<vector_field>& flow_map);
  void                          assign    (                                    const std::vector<particle>& particles,       std::unique_ptr<vector_field>& flow_map);
};
}

#endif