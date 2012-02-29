//
//  Codec.h
//  LoungeAct
//
//  Created by Casey Marshall on 2/27/12.
//  Copyright (c) 2012 Memeo, Inc. All rights reserved.
//

#ifndef LoungeAct_Codec_h
#define LoungeAct_Codec_h

#include "../Storage/ObjectStore.h"

typedef void *Value;

int value_is_object(Value value);
int value_is_array(Value value);
int value_is_integer(const json_t *json);
int value_is_real(const json_t *json);
int value_is_true(const json_t *json);
int value_is_false(const json_t *json);
int value_is_null(const json_t *json);

#endif
