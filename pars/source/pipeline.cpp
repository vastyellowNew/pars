#include <pars/pipeline.hpp>

#include <bm/bm.hpp>
#include <tbb/tbb.h>

#include <pa/stages/flow_map_generator.hpp>
#include <pa/stages/ftle_map_generator.hpp>
#include <pa/stages/color_generator.hpp>
#include <pa/stages/index_generator.hpp>
#include <pa/stages/seed_generator.hpp>

namespace pars
{
pipeline::pipeline(const std::size_t thread_count) : partitioner_(&communicator_), data_loader_(&partitioner_), particle_tracer_(&partitioner_), ray_tracer_(&partitioner_, thread_count)
{

}

std::pair<image, bm::mpi_session<>> pipeline::execute     (const settings& settings)
{
             std::optional<pa::scalar_field>     local_scalar_field    ;
             std::optional<pa::vector_field>     local_vector_field    ;
  std::array<std::optional<pa::vector_field>, 6> neighbor_vector_fields;
  std::vector<pa::particle>                      seeds                 ;
  std::vector<pa::integral_curves>               integral_curves       ;
  
  auto volume_support     = settings.mode().find("volume"     ) != std::string::npos;
  auto streamline_support = settings.mode().find("streamlines") != std::string::npos;

  auto session = bm::run_mpi<double, std::milli>([&] (bm::session_recorder<double, std::milli>& recorder)
  {
    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "1.0::data_loader::set_file\n";
    recorder.record("1.0::data_loader::set_file"                   , [&] ()
    {
      data_loader_.set_file(settings.dataset_filepath());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "1.1::data_loader::load_dimensions\n";
    recorder.record("1.1::data_loader::load_dimensions"            , [&] ()
    {
      auto dimensions = data_loader_.load_dimensions();
      partitioner_.set_domain_size({dimensions[0], dimensions[1], dimensions[2]});
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "1.2::data_loader::load_local_scalar_field\n";
    recorder.record("1.2::data_loader::load_local_scalar_field"    , [&] ()
    {
      if (!volume_support)
        return;

      local_scalar_field     = data_loader_.load_local_scalar_field(settings.volume_type());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "1.3::data_loader::load_local_vector_field\n";
    recorder.record("1.3::data_loader::load_local_vector_field"    , [&] ()
    {
      if (!streamline_support)
        return;

      local_vector_field     = data_loader_.load_local_vector_field();
    });
    if (communicator_.rank() == 0) std::cout << "1.4::data_loader::load_neighbor_vector_fields\n";
    recorder.record("1.4::data_loader::load_neighbor_vector_fields", [&]()
    {
      if (!streamline_support)
        return;

      if (settings.particle_tracing_load_balance())
        neighbor_vector_fields = data_loader_.load_neighbor_vector_fields();
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "2.0::seed_generator::generate\n";
    recorder.record("2.0::seed_generator::generate"       , [&] ()
    {
      if (!streamline_support)
        return;

      pa::vector3 stride(settings.seed_generation_stride(0), settings.seed_generation_stride(1), settings.seed_generation_stride(2));
      seeds = pa::seed_generator::generate(
        local_vector_field->offset,
        local_vector_field->size  ,
        local_vector_field->spacing.array() * stride.array(),
        settings.seed_generation_iterations(),
        communicator_.rank());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "3.0::particle_tracer::initialize\n";
    recorder.record("3.0::particle_tracer::initialize"    , [&] ()
    {
      if (!streamline_support)
        return;

      particle_tracer_.set_local_vector_field    (&local_vector_field    );
      particle_tracer_.set_neighbor_vector_fields(&neighbor_vector_fields);
      particle_tracer_.set_step_size             (settings.particle_tracing_step_size());
      if      (settings.particle_tracing_integrator() == std::string("euler"))
        particle_tracer_.set_integrator(pa::euler_integrator                       ());
      else if (settings.particle_tracing_integrator() == std::string("modified_midpoint"))
        particle_tracer_.set_integrator(pa::modified_midpoint_integrator           ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_4"))
        particle_tracer_.set_integrator(pa::runge_kutta_4_integrator               ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_cash_karp_54"))
        particle_tracer_.set_integrator(pa::runge_kutta_cash_karp_54_integrator    ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_dormand_prince_5"))
        particle_tracer_.set_integrator(pa::runge_kutta_dormand_prince_5_integrator());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_fehlberg_78"))
        particle_tracer_.set_integrator(pa::runge_kutta_fehlberg_78_integrator     ());
      else if (settings.particle_tracing_integrator() == std::string("adams_bashforth_2"))
        particle_tracer_.set_integrator(pa::adams_bashforth_2_integrator           ());
      else if (settings.particle_tracing_integrator() == std::string("adams_bashforth_moulton_2"))
        particle_tracer_.set_integrator(pa::adams_bashforth_moulton_2_integrator   ());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "3.1::particle_tracer::trace\n";
    if (streamline_support)
    {
      pa::integer round_counter = 0;
      bool        complete      = false;
      while (!complete)
      {
        pa::particle_tracer::round_info round_info;

        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".0::particle_tracer::load_balance_distribute\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".0::particle_tracer::load_balance_distribute"   , [&]()
        {
          if (settings.particle_tracing_load_balance())
                       particle_tracer_.load_balance_distribute (seeds                             );
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".1::particle_tracer::compute_round_info\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".1::particle_tracer::compute_round_info"        , [&]()
        {
          round_info = particle_tracer_.compute_round_info      (seeds, integral_curves            );
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".2::particle_tracer::allocate\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".2::particle_tracer::allocate"                  , [&]()
        {
                       particle_tracer_.allocate                (       integral_curves, round_info);
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".3::particle_tracer::initialize\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".3::particle_tracer::initialize"                , [&]()
        {
                       particle_tracer_.initialize              (seeds, integral_curves, round_info);
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".4::particle_tracer::trace\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".4::particle_tracer::trace"                     , [&]()
        {
                       particle_tracer_.trace                   (seeds, integral_curves, round_info);
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".5::particle_tracer::load_balance_collect\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".5::particle_tracer::load_balance_collect"      , [&]()
        {
          if (settings.particle_tracing_load_balance())
                       particle_tracer_.load_balance_collect    (                        round_info);
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".6::particle_tracer::out_of_bounds_redistribute\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".6::particle_tracer::out_of_bounds_redistribute", [&]()
        {
                       particle_tracer_.out_of_bounds_distribute(seeds,                  round_info);
        });
        // if (communicator_.rank() == 0) std::cout << "3.1." + std::to_string(round_counter) + ".7::particle_tracer::check_completion\n";
        recorder.record("3.1." + std::to_string(round_counter) + ".7::particle_tracer::check_completion"          , [&]()
        {
          complete   = particle_tracer_.check_completion        (seeds                             );
        });

        round_counter++;
      }
    }

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "3.2::particle_tracer::prune\n";
    recorder.record("3.2::particle_tracer::prune"         , [&] ()
    {
      if (!streamline_support)
        return;

      particle_tracer_.prune (integral_curves);
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "4.0::index_generator::generate\n";
    recorder.record("4.0::index_generator::generate"      , [&] ()
    {
      if (!streamline_support)
        return;

      tbb::parallel_for(std::size_t(0), integral_curves.size(), std::size_t(1), [&] (const std::size_t index)
      {
        pa::index_generator::generate(&integral_curves[index]);
      });
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "5.0::color_generator::generate\n";
    recorder.record("5.0::color_generator::generate"      , [&] ()
    {
      if (!streamline_support)
        return;

      pa::color_generator::mode mode;
      if      (settings.color_generation_mode() == std::string("hsl_constant_s"))
        mode = pa::color_generator::mode::hsl_constant_s;
      else if (settings.color_generation_mode() == std::string("hsl_constant_l"))
        mode = pa::color_generator::mode::hsl_constant_l;
      else if (settings.color_generation_mode() == std::string("hsv_constant_s"))
        mode = pa::color_generator::mode::hsv_constant_s;
      else if (settings.color_generation_mode() == std::string("hsv_constant_v"))
        mode = pa::color_generator::mode::hsv_constant_v;
      else if (settings.color_generation_mode() == std::string("rgb"))
        mode = pa::color_generator::mode::rgb;

      tbb::parallel_for(std::size_t(0), integral_curves.size(), std::size_t(1), [&] (const std::size_t index)
      {
        pa::color_generator::generate(&integral_curves[index], mode, settings.color_generation_free_parameter());
      });
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.0::ray_tracer::set_camera\n";
    recorder.record("6.0::ray_tracer::set_camera"         , [&] ()
    {
      ray_tracer_.set_camera(
        {settings.raytracing_camera_position(0), settings.raytracing_camera_position(1), settings.raytracing_camera_position(2)},
        {settings.raytracing_camera_forward (0), settings.raytracing_camera_forward (1), settings.raytracing_camera_forward (2)},
        {settings.raytracing_camera_up      (0), settings.raytracing_camera_up      (1), settings.raytracing_camera_up      (2)});
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.1::ray_tracer::set_image_size\n";
    recorder.record("6.1::ray_tracer::set_image_size"     , [&] ()
    {
      ray_tracer_.set_image_size({ settings.raytracing_image_size(0), settings.raytracing_image_size(1) });
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.2::ray_tracer::set_volume\n";
    recorder.record("6.2::ray_tracer::set_volume"         , [&] ()
    {
      if (!volume_support)
        return;

      ray_tracer_.set_volume(&local_scalar_field.value());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.3::ray_tracer::set_integral_curves\n";
    recorder.record("6.3::ray_tracer::set_integral_curves", [&] ()
    {
      if (!streamline_support)
        return;

      ray_tracer_.set_integral_curves(&integral_curves, settings.raytracing_streamline_radius() ? settings.raytracing_streamline_radius() : 1.0f);
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.4::ray_tracer::trace\n";
    recorder.record("6.4::ray_tracer::trace"              , [&] ()
    {
      ray_tracer_.trace(settings.raytracing_iterations());
    });

    communicator_.barrier();

    if (communicator_.rank() == 0) std::cout << "6.5::ray_tracer::serialize\n";
  });

  return {ray_tracer_.serialize(), session};
}
                 bm::mpi_session<>  pipeline::execute_ftle(const settings& settings)
{
  pa::flow_map_generator flow_map_generator(&partitioner_);
  pa::ftle_map_generator ftle_map_generator(&partitioner_);

  std::optional  <pa::vector_field> vector_field;
  std::unique_ptr<pa::vector_field> flow_map    ;
  std::unique_ptr<pa::scalar_field> ftle_map    ;

  auto session = bm::run_mpi<double, std::milli>([&] (bm::session_recorder<double, std::milli>& recorder)
  {
    if (communicator_.rank() == 0) std::cout << "1.0::data_loader::set_file\n";
    recorder.record("1.0::data_loader::set_file"               , [&] ()
    {
      data_loader_.set_file(settings.dataset_filepath());
    });
    if (communicator_.rank() == 0) std::cout << "1.1::data_loader::load_dimensions\n";
    recorder.record("1.1::data_loader::load_dimensions"        , [&] ()
    {
      auto dimensions = data_loader_.load_dimensions();
      partitioner_.set_domain_size({dimensions[0], dimensions[1], dimensions[2]});
    });
    if (communicator_.rank() == 0) std::cout << "1.2::data_loader::load_local_vector_field\n";
    recorder.record("1.2::data_loader::load_local_vector_field", [&] ()
    {
      vector_field = data_loader_.load_local_vector_field();
    });

    if (communicator_.rank() == 0) std::cout << "2.0::flow_map_generator::initialize\n";
    recorder.record("2.0::flow_map_generator::initialize"      , [&] ()
    {
      flow_map_generator.set_vector_field(&vector_field.value());
      flow_map_generator.set_step_size   (settings.particle_tracing_step_size());
      if      (settings.particle_tracing_integrator() == std::string("euler"))
        flow_map_generator.set_integrator(pa::euler_integrator                       ());
      else if (settings.particle_tracing_integrator() == std::string("modified_midpoint"))
        flow_map_generator.set_integrator(pa::modified_midpoint_integrator           ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_4"))
        flow_map_generator.set_integrator(pa::runge_kutta_4_integrator               ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_cash_karp_54"))
        flow_map_generator.set_integrator(pa::runge_kutta_cash_karp_54_integrator    ());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_dormand_prince_5"))
        flow_map_generator.set_integrator(pa::runge_kutta_dormand_prince_5_integrator());
      else if (settings.particle_tracing_integrator() == std::string("runge_kutta_fehlberg_78"))
        flow_map_generator.set_integrator(pa::runge_kutta_fehlberg_78_integrator     ());
      else if (settings.particle_tracing_integrator() == std::string("adams_bashforth_2"))
        flow_map_generator.set_integrator(pa::adams_bashforth_2_integrator           ());
      else if (settings.particle_tracing_integrator() == std::string("adams_bashforth_moulton_2"))
        flow_map_generator.set_integrator(pa::adams_bashforth_moulton_2_integrator   ());
    });
    if (communicator_.rank() == 0) std::cout << "2.1::flow_map_generator::generate\n";
    recorder.record("2.1::flow_map_generator::generate"        , [&]()
    {
      flow_map = flow_map_generator.generate(settings.seed_generation_iterations(), pa::scalar(1) / settings.seed_generation_stride()[0]);
    });
    
    if (communicator_.rank() == 0) std::cout << "3.0::ftle_map_generator::initialize\n";
    recorder.record("3.0::ftle_map_generator::initialize"      , [&] ()
    {
      ftle_map_generator.set_flow_map(flow_map.get());
      ftle_map_generator.set_time    (settings.particle_tracing_step_size() * settings.seed_generation_iterations());
    });
    if (communicator_.rank() == 0) std::cout << "3.1::ftle_map_generator::generate\n";
    recorder.record("3.1::ftle_map_generator::generate"        , [&] ()
    {
      ftle_map = ftle_map_generator.generate();
    });
    
    if (communicator_.rank() == 0) std::cout << "4.0::data_loader::save_ftle_map\n";
    recorder.record("4.0::data_loader::save_ftle_map"          , [&] ()
    {
      data_loader_.save_ftle_field("ftle", ftle_map.get());
    });
  });

  return session;
}

boost::mpi::communicator*           pipeline::communicator()
{
  return &communicator_;
}
}