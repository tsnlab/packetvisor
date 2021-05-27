#!/bin/bash

# Tag name must be v(major).(minor) format
TAG=`git describe --tags --long`

# Parse major, minor, micro and commit
MAJOR=`echo ${TAG:1} | awk -F- '{print $1}' | awk -F. '{print $1}'` 
MINOR=`echo ${TAG:1} | awk -F- '{print $1}' | awk -F. '{print $2}'` 
MICRO=`echo ${TAG:1} | awk -F- '{print $2}'`
COMMIT=`echo ${TAG:1} | awk -F- '{print $3}'`
COMMIT=${COMMIT:1:7}

cat << EOF > src/ver.h
#ifndef __PV_VERSION__
#define __PV_VERSION__

#define PV_VERSION_MAJOR ${MAJOR}
#define PV_VERSION_MINOR ${MINOR}
#define PV_VERSION_MICRO ${MICRO}
#define PV_VERSION_COMMIT ${COMMIT}
#define PV_VERSION "${MAJOR}.${MINOR}.${MICRO}-${COMMIT}"

#endif /* __PV_VERSION__ */
EOF
