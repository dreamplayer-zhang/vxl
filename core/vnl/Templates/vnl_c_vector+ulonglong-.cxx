#include <vcl_compiler.h>
#if VCL_HAS_LONG_LONG
// Disable warning
#ifdef VCL_VC
// 4146: unary minus operator applied to unsigned type, result still unsigned
# pragma warning(disable:4146)
#endif //VCL_VC
#include <vnl/vnl_c_vector.hxx>
VNL_C_VECTOR_INSTANTIATE_ordered(unsigned long long);
#else
void vnl_c_vector_ulonglong_dummy(void) {}
#endif
