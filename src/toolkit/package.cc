
#include "package.h"

namespace cyclus {
namespace toolkit {

Package::Package() {
    package_id_ = "";
}

Package::Package(std::string package_id_) : package_id_(package_id) {}

Package::~Package() {}

void Package::set_package_id(std::string package_id_) {
    package_id_ = package_id;
}

}  // namespace toolkit
}  // namespace cyclus