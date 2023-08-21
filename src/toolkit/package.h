#ifndef CYCLUS_SRC_TOOLKIT_PACKAGE_H_
#define CYCLUS_SRC_TOOLKIT_PACKAGE_H_

#include <string>

#include "material.h"

namespace cyclus {
namespace toolkit {

}  // namespace toolkit
}  // namespace cyclus

/// Packaging for a material
class Package {
    public:
        /// Default constructor
        Package ();

        // Constructor with package id
        Package (std::string package_id);

        /// Default destructor for Package
        ~Package();

        void set_package_id(std::string package_id);

    private:
        /// Package ID
        std::string package_id_;
};

#endif  // CYCLUS_SRC_TOOLKIT_PACKAGE_H_