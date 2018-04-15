# rddvb
DVB auto-scheduler

rddvb is a Linux based DVB auto-scheduler, which uses several standard Linux programs to provide modern PVR functionality with advanced searching and scheduling of DVB programmes.

rddvb does NOT provide playback facilities, although the provided web interface can be used to playback content in a browser.  The author recommends Plex Media Server for playback facilities.  Alternatively, files can be copied to the local drive and watched using VLC or similar.

Using XMLTV, listings are downloaded (usually every day), searched using patterns to create a list of 'requested' programmes, from this list, programmes that cannot be recorded or have already been recorded are limited.  The remainder are sorted and prioritized according to explicit and implicit priorities and scheduled (using 'at').

Recording of DVB programmes is done using dvbstream thereby remaining compatible with both DVB-T and DVB-S.  The resultant mpeg-2 file is split using projectx into audio and video tracks. During this process, the programme is also split into sections of different aspect ratios. All the sections of the most popular aspect ratio (i.e. the longest accumlative time) are concatenated back together and converted into mp4 using avconv.

Resultant programmes are stored in a configurable directory structure, including a user who the original search pattern belongs to.

A web interface is also provided, running under apache (or any other webserver), which allows viewing of listings, requested and recorded programmes, editing of the existing search patterns and adding new ones.

