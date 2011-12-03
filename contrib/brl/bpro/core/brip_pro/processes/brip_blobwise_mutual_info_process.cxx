// This is brl/bpro/core/brip_pro/processes/brip_blobwise_mutual_info_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief  process to calculate blob wise mutual information between expected and input image

#include <brip/brip_vil_float_ops.h>
#include <brip/brip_blobwise_mutual_info.h>
#include <bprb/bprb_parameters.h>
#include <vil/vil_image_view_base.h>
#include <vil/vil_new.h>
#include <vil/vil_math.h>
#include <vil/vil_convert.h>
#include <vil/vil_save.h>
#include <vil/algo/vil_binary_dilate.h>
#include <vil/algo/vil_binary_erode.h>

//: global variables
namespace brip_blobwise_mutual_info_process_globals
{
  const unsigned n_inputs_  = 3;
  const unsigned n_outputs_ = 1;
  
  
  // private helper method prepares an input image to be processed by update
  vil_image_view<float>* prepare_input_image(vil_image_view_base_sptr loaded_image) {
    //then it's an RGB image (assumes byte image...)
    if (loaded_image->nplanes() == 3 || loaded_image->nplanes() == 4)
    {
      vcl_cout<<"preparing rgb as input to grey scale float image"<<vcl_endl;

      //load image from file and format it into grey
      vil_image_view<vxl_byte>* inimg    = dynamic_cast<vil_image_view<vxl_byte>* >(loaded_image.ptr());
      vil_image_view<float>     gimg(loaded_image->ni(), loaded_image->nj());
      vil_convert_planes_to_grey<vxl_byte, float>(*inimg, gimg);

      //stretch it into 0-1 range
      vil_image_view<float>*    floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj());
      vil_convert_stretch_range_limited(gimg, *floatimg, 0.0f, 255.0f, 0.0f, 1.0f);
      return floatimg;
    }

    //else if loaded planes is just one...
    if (loaded_image->nplanes() == 1)
    {
      vcl_cout<<"Preparing grey scale image"<<vcl_endl;

      //preapre floatimg for stretched img
      vil_image_view<float>* floatimg;
      if (vil_image_view<vxl_byte> *img_byte = dynamic_cast<vil_image_view<vxl_byte>*>(loaded_image.ptr()))
      {
        floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj(), 1);
        vil_convert_stretch_range_limited(*img_byte, *floatimg, vxl_byte(0), vxl_byte(255), 0.0f, 1.0f);
      }
      else if (vil_image_view<unsigned short> *img_byte = dynamic_cast<vil_image_view<unsigned short>*>(loaded_image.ptr()))
      {
        floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj(), 1);
        vil_convert_stretch_range_limited(*img_byte, *floatimg,(unsigned short)30500,(unsigned short)32500,  0.0f, 1.0f); // hardcoded to be fixed.
      }
      else if (vil_image_view<float> *img_float = dynamic_cast<vil_image_view<float>*>(loaded_image.ptr()))
      {
        return img_float;
      }
      else {
        vcl_cerr << "Failed to load image\n";
        return 0;
      }
      return floatimg;
    }
    //otherwise it's messed up, return a null pointer
    vcl_cerr<<"Failed to recognize input image type\n";
    return 0;
  }

}; 

//: Constructor
bool brip_blobwise_mutual_info_process_cons(bprb_func_process& pro)
{
  //input
  vcl_vector<vcl_string> input_types;
  input_types.push_back("vil_image_view_base_sptr"); // input image
  input_types.push_back("vil_image_view_base_sptr"); // expected image
  input_types.push_back("vil_image_view_base_sptr"); // weight image
  input_types.push_back("vil_image_view_base_sptr"); // blob/mask image
  bool good = pro.set_input_types(input_types);
  if(!good) return good; 

  //output
  vcl_vector<vcl_string> output_types;
  output_types.push_back("vil_image_view_base_sptr");  // visualize image (mutual informaiton differs by color
  good= pro.set_output_types(output_types);
  if (!good) return good;
  
  return true;
}


//: Execute the process
bool brip_blobwise_mutual_info_process(bprb_func_process& pro)
{
  using namespace brip_blobwise_mutual_info_process_globals;
  
  // Sanity check
  if (pro.n_inputs() < n_inputs_) {
    vcl_cout << "brip_blobwise_mutual_info_process: The input number should be " << n_inputs_ << vcl_endl;
    return false;
  }

  // get the inputs
  unsigned i=0;
  vil_image_view_base_sptr in_img_ptr   = pro.get_input<vil_image_view_base_sptr>(i++);
  vil_image_view_base_sptr exp_img_ptr  = pro.get_input<vil_image_view_base_sptr>(i++);
  vil_image_view_base_sptr weight_ptr   = pro.get_input<vil_image_view_base_sptr>(i++);
  vil_image_view_base_sptr blob_img_ptr = pro.get_input<vil_image_view_base_sptr>(i++);
  
  //prepare input images
  vil_image_view<float>*  in_img  = prepare_input_image(in_img_ptr); 
  vil_image_view<float>*  exp_img = prepare_input_image(exp_img_ptr);
  vil_image_view<float>*  weight_img = prepare_input_image(weight_ptr);
  
  //mask image should be a byte image...
  unsigned ni=blob_img_ptr->ni(), nj=blob_img_ptr->nj();
  vil_image_view<vxl_byte>* blob_img = static_cast<vil_image_view<vxl_byte>* >(blob_img_ptr.ptr()); 
  vil_image_view<bool> mask_img(ni, nj); 
  for(int i=0; i<ni; ++i)
    for(int j=0; j<nj; ++j)
      mask_img(i,j) = ( (*blob_img)(i,j) > 0 ) ? true : false; 
      
  //dilate mask image a bit before
  vil_image_view<bool> mask_dest(ni,nj), mask_dest1(ni,nj);
  vil_structuring_element selem;
  selem.set_to_disk(1.05);
  vil_binary_dilate(mask_img, mask_dest, selem);
  vil_binary_dilate(mask_dest, mask_dest1, selem);
  vil_binary_dilate(mask_dest1, mask_dest, selem);

  //calculate per blob mutual information 
  vil_image_view<float>* mi_image = new vil_image_view<float>(in_img->ni(), in_img->nj()) ;
  mi_image->fill(0.0f); 
  //brip_blobwise_mutual_info(*in_img, *exp_img, *weight_img, mask_dest, *mi_image); 
  brip_blobwise_kl_div(*in_img, *exp_img, mask_dest, *mi_image); 
  
  //find min max on Mutual Info
  float min_value, max_value; 
  vil_math_value_range(*mi_image, min_value, max_value); 
  vil_save(*mi_image, "kl_div.tiff");
  
  //create new blob info (threshold the mi image by some value)
  float thresh = .1 * max_value; 
  vil_image_view<vxl_byte>* new_blobs = new vil_image_view<vxl_byte>(ni,nj); 
  for(int i=0; i<ni; ++i) 
    for(int j=0; j<nj; ++j) {
      if( mask_img(i,j) && (*mi_image)(i,j) > thresh) 
        (*new_blobs)(i,j) = (vxl_byte) 255; 
      else
        (*new_blobs)(i,j) = (vxl_byte) 0; 
    }
  
  //debug save
  delete mi_image; 
  
  i=0;
  pro.set_output_val<vil_image_view_base_sptr>(i++, new_blobs);
  return true;
}
