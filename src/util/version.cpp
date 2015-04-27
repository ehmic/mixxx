#include "util/version.h"

#include "defs_version.h"
#include "build.h" // Generated by SCons

// static
QString Version::version() {
    return VERSION;
}

// static
QString Version::applicationTitle() {
#ifdef __APPLE__
    return "Mixxx";
#elif defined(AMD64) || defined(EM64T) || defined(x86_64)
    return QString("Mixxx %1 x64").arg(VERSION);
#elif defined(IA64)
    return QString("Mixxx %1 Itanium").arg(VERSION);
#else
    return QString("Mixxx %1").arg(VERSION);
#endif
}

// static
QString Version::developmentBranch() {
#ifdef BUILD_BRANCH
    return BUILD_BRANCH;
#else
    return QString();
#endif
}

// static
QString Version::developmentRevision() {
#ifdef BUILD_REV
    return BUILD_REV;
#else
    return QString();
#endif
}

// static
QString Version::buildFlags() {
#ifdef BUILD_FLAGS
    return BUILD_FLAGS;
#else
    return QString();
#endif
}
