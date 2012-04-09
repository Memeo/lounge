#!/bin/sh

#  la_config.sh
#  LoungeAct
#
#  Created by Casey Marshall on 4/7/12.
#  Copyright (c) 2012 Memeo, Inc. All rights reserved.

./configure --prefix=/Users/csm/local/nginx-loungeact --add-module=/Users/csm/Source/LoungeAct/ngx_http_loungeact --with-ld-opt="-L/Users/csm/Library/Developer/Xcode/DerivedData/LoungeAct-hiwbfamibjtzqyekubhwduhmpjfi/Build/Products/Release/ -L/Users/csm/Library/Developer/Xcode/DerivedData/LoungeAct-hiwbfamibjtzqyekubhwduhmpjfi/Build/Products/Release/db-5.3.15/lib" --with-cc-opt="-I/Users/csm/Source/LoungeAct -I/Users/csm/Source/LoungeAct/jansson -I/Users/csm/Source/LoungeAct/jansson/libtommath -I/Users/csm/Source/LoungeAct/ngx_http_loungeact" --with-debug