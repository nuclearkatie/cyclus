#ifndef CYCLUS_SRC_TOOLKIT_PACKAGE_H_
#define CYCLUS_SRC_TOOLKIT_PACKAGE_H_

#include <string>

namespace cyclus {
namespace toolkit {

}  // namespace toolkit
}  // namespace cyclus

/// Packaging for a material
class Package {
    public:
        /// default constructor
        Package ();

        /// constructor
        /// @param package_id the package id
        Package (std::string package_id);

        /// destructor
        ~Package();

        /// change the package_id 
        void set_package_id(std::string package_id);

    private:
        /// Package ID
        std::string package_id_;
};

#endif  // CYCLUS_SRC_TOOLKIT_PACKAGE_H_