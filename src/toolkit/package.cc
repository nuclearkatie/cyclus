
#include "package.h"

namespace cyclus {
namespace toolkit {

int Package::next_package_id_ = 1;

Package::Package() {
    package_type_ = "";
}

Package::Package(PackageType package_type) : package_type_(package_type) {}

Package::~Package() {}

}  // namespace toolkit
}  // namespace cyclus