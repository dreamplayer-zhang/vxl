#include <iostream>
#include <vector>
#include <vtol/vtol_face.h>
#include <vtol/vtol_list_functions.hxx>
#ifdef _MSC_VER
#  include "vcl_msvc_warnings.h"
#endif

template std::vector<vtol_face*>* tagged_union(std::vector<vtol_face*>*);
