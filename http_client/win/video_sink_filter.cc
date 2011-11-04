// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "baseclasses/streams.h"

// TODO(tomfinegan): webrtc uses baseclasses, but has worked around the need
//                   for the next two lines. Determining how to do so would be
//                   enlightening, but isn't that important.
//                   Without these two lines dllentry.cpp from the baseclasses
//                   sources will cause an error at link time (LNK2001,
//                   unresolved external symbol) because of use of the following
//                   two globals via extern.
CFactoryTemplate* g_Templates = NULL;
int g_cTemplates = 0;
