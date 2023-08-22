#ifndef CYCLUS_SRC_TOOLKIT_PACKAGE_H_
#define CYCLUS_SRC_TOOLKIT_PACKAGE_H_

#include <string>

namespace cyclus {
namespace toolkit {

typedef std::string PackageType;

/// Packaging can be applied and removed from a material to represent
/// physical containers that hold nuclear materials.
class Package {
    public:
        /// default constructor
        Package () : package_id_(next_package_id_++) {}

        /// constructor
        /// @param package_type_ the name of the package
        Package (PackageType package_type_) : package_id_(next_package_id_++) {};

        /// destructor
        ~Package();

        /// returns the unique id corresponding to this package. Can be used
        /// to track and/or associate other information with this package.
        const int package_id() const { return package_id_; }

    private:
        static int next_package_id_;
        int package_id_;
        PackageType package_type_;
};

}  // namespace toolkit
}  // namespace cyclus

#endif  // CYCLUS_SRC_TOOLKIT_PACKAGE_H_