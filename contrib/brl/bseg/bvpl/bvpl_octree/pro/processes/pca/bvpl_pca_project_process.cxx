//:
// \file
// \brief A process to compute pca projection coeffiecients
// \author Isabel Restrepo
// \date 28-Feb-2011

#include <bprb/bprb_func_process.h>
#include <bprb/bprb_parameters.h>

#include <brdb/brdb_value.h>

#include <bvpl/bvpl_octree/bvpl_global_pca.h>

//:global variables
namespace bvpl_pca_project_process_globals
{
  const unsigned n_inputs_ = 5;
  const unsigned n_outputs_ = 0;
}


//:sets input and output types
bool bvpl_pca_project_process_cons(bprb_func_process& pro)
{
  using namespace bvpl_pca_project_process_globals ;

  std::vector<std::string> input_types_(n_inputs_);
  unsigned i = 0;
  input_types_[i++] = "bvpl_global_pca_125_sptr" ; //global pca class
  input_types_[i++] = "int"; //scene id
  input_types_[i++] = "int";  //block Indices
  input_types_[i++] = "int";
  input_types_[i++] = "int";

  std::vector<std::string> output_types_(n_outputs_);

  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}


//:the process
bool bvpl_pca_project_process(bprb_func_process& pro)
{
  using namespace bvpl_pca_project_process_globals;

  //get inputs
  unsigned i = 0;
  bvpl_global_pca_125_sptr global_pca = pro.get_input<bvpl_global_pca_125_sptr>(i++);
  int scene_id = pro.get_input<int>(i++);
  int block_i = pro.get_input<int>(i++);
  int block_j = pro.get_input<int>(i++);
  int block_k = pro.get_input<int>(i++);

  if (!global_pca)
  {
    std::cerr << "Global PCA is NULL\n";
    return false;
  }

  global_pca->project(scene_id, block_i, block_j, block_k);

  return true;
}
