
#--------------------------------------------------------------------------------
# Copyright (c) 2012-2013, Lars Baehren <lbaehren@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#--------------------------------------------------------------------------------

# - Check for the presence of YAML
#
# The following variables are set when YAML is found:
#  YAML_FOUND      = Set to true, if all components of YAML have been found.
#  YAML_INCLUDES   = Include path for the header files of YAML
#  YAML_LIBRARIES  = Link these to use YAML
#  YAML_LFLAGS     = Linker flags (optional)

if (NOT YAML_FOUND)

  if (NOT YAML_ROOT_DIR)
    set (YAML_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
  endif (NOT YAML_ROOT_DIR)

  ##_____________________________________________________________________________
  ## Check for the header files

  find_path (YAML_INCLUDES yaml-cpp/yaml.h yaml-cpp/node.h
    HINTS ${YAML_ROOT_DIR} ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES include
    )

  ##_____________________________________________________________________________
  ## Check for the library

  find_library (YAML_LIBRARIES yaml-cpp
    HINTS ${YAML_ROOT_DIR} ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES lib
    )

  ##_____________________________________________________________________________
  ## Actions taken when all components have been found

  find_package_handle_standard_args (YAML DEFAULT_MSG YAML_LIBRARIES YAML_INCLUDES)

  if (YAML_INCLUDES AND YAML_LIBRARIES)
    set (YAML_FOUND TRUE)
  else (YAML_INCLUDES AND YAML_LIBRARIES)
    set (YAML_FOUND FALSE)
    if (NOT YAML_FIND_QUIETLY)
      if (NOT YAML_INCLUDES)
	message (STATUS "Unable to find YAML header files!")
      endif (NOT YAML_INCLUDES)
      if (NOT YAML_LIBRARIES)
	message (STATUS "Unable to find YAML library files!")
      endif (NOT YAML_LIBRARIES)
    endif (NOT YAML_FIND_QUIETLY)
  endif (YAML_INCLUDES AND YAML_LIBRARIES)

  if (YAML_FOUND)
    if (NOT YAML_FIND_QUIETLY)
      message (STATUS "Found components for YAML")
      message (STATUS "YAML_ROOT_DIR  = ${YAML_ROOT_DIR}")
      message (STATUS "YAML_INCLUDES  = ${YAML_INCLUDES}")
      message (STATUS "YAML_LIBRARIES = ${YAML_LIBRARIES}")
    endif (NOT YAML_FIND_QUIETLY)
  else (YAML_FOUND)
    if (YAML_FIND_REQUIRED)
      message (FATAL_ERROR "Could not find YAML!")
    endif (YAML_FIND_REQUIRED)
  endif (YAML_FOUND)

  ## Compatibility setting
  set (YAML_CPP_FOUND ${YAML_FOUND})

  ##_____________________________________________________________________________
  ## Mark advanced variables

  mark_as_advanced (
    YAML_ROOT_DIR
    YAML_INCLUDES
    YAML_LIBRARIES
    )

endif (NOT YAML_FOUND)
