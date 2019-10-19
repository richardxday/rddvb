(c++-mode . ((filetypes . (cpp-project))
	     (includes . ("include"
			  "/usr/include/postgresql"))
	     (defines . ("RDDVB_SHARE_DIR=/usr/local/share/rddvb-0.1"))
	     (flags . ("-std=c++11"))
	     (warnings . ("all"
			  "extra"))
	     (packages . ("rdlib-0.1"
			  "jsoncpp"))
	     (configcommands . ("curl-config --cflags"))
	     (sources . ("src"
			 "../rdlib/include"
			 "../rdlib/src"))
	     (buildcmd . "make -f makefile")))
