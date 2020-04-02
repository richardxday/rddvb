(c++-mode . ((filetypes . (cpp-project))
             (includes . ("include"
                          "/usr/include/postgresql"))
             (defines . ("RDDVB_ROOT_DIR=\"/\""
                         "RDDVB_SHARE_DIR=\"/usr/local/share/rddvb-0.1\""))
             (flags . ("-xc++"
                       "-std=c++11"))
             (packages . ("rdlib-0.1"
                          "jsoncpp"))
             (configcommands . ("curl-config --cflags"))
             (sources . ("src"
                         "../rdlib/include"
                         "../rdlib/src"))
             (buildcmd . "make -f makefile")))
