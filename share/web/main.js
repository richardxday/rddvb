var defaultuser = 'default';
var defaulttimefilter = 'start>=midnight,yesterday';

var showchannelicons = true;
var showprogicons = false;
var iconimgstyle = 'max-width:60px;max-height:60px';
var iconimgbigstyle = 'max-width:170px;max-height:170px';
var daynames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
var monthnames = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
var response = null;
var patterns = [];
var editingpattern = -1;
var searchpattern = null;
var patterndefs = null;
var xmlhttp = null;
var currentfilter = null;
var graphsuffix = 'png';
var defaultsortfields = "none"
var filters = [
    {text:"Sources:<br>"},
    {
        title:"Listings",
        filter:{from:"Listings",titlefilter:"",timefilter:"start>now",sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Combined",
        filter:{from:"Combined",titlefilter:"",timefilter:defaulttimefilter,sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Requested",
        filter:{from:"Requested",titlefilter:"",timefilter:"",sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Rejected",
        filter:{from:"Combined",titlefilter:"rejected=1",timefilter:defaulttimefilter,sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Scheduled",
        filter:{from:"Combined",titlefilter:"scheduled=1",timefilter:defaulttimefilter,sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Recorded",
        filter:{from:"Recorded",titlefilter:"",timefilter:defaulttimefilter,sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Failed",
        filter:{from:"Failures",titlefilter:"",timefilter:defaulttimefilter,sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Titles (Combined)",
        filter:{from:"Titles (Combined)",titlefilter:"",timefilter:"",sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Titles (Listings)",
        filter:{from:"Titles (Listings)",titlefilter:"",timefilter:"",sortfields:defaultsortfields,expanded:-1,fetch:true},
    },
    {
        title:"Patterns",
        filter:{from:"Patterns",expanded:-1,fetch:true},
    },
    {
        title:"Clear Filter",
        filter:{titlefilter:"",expanded:-1,fetch:true},
    },
    {text:"<br>DVB Logs:<br>"},
    {
        title:"Today",
        filter:{from:"Logs",timefilter:"start=today",sortfields:"",expanded:-1,fetch:true},
    },
    {
        title:"Yesterday",
        filter:{from:"Logs",timefilter:"start=yesterday",sortfields:"",expanded:-1,fetch:true},
    },
];
var searches        = null;
var stats           = null;
var link            = null;
var globals         = null;
var channels        = null;
var proglistelement = '';
var ua              = navigator.userAgent.toLowerCase();
var isAndroid       = (ua.indexOf("android") >= 0); //&& ua.indexOf("mobile");

var videointents = [
    {
        package: "com.mxtech.videoplayer.pro",
        name: "MX Player",
    },
    {
        package: "org.videolan.vlc",
        name: "VLC",
        action: "android.intent.action.VIEW",
        type: "video/*",
    },
];

var waitingforstreamurlstimer = null;

window.onpopstate = function(event)
{
    var filter = event.state;

    if (filter != null) {
        dvbrequest(filter, '', false);
    }
};

function loadpage()
{
    var i, str = '';

    for (i = 0; i < filters.length; i++) {
        if (typeof filters[i].text != 'undefined') str += filters[i].text;
        else {
            str += '<button onclick="dvbrequest(filters[' + i + '].filter)"';
            if (typeof filters[i].title != 'undefined') str += ' title="' + filters[i].title + '"';
            str += '>' + filters[i].title + '</button><br>';
        }
    }

    document.getElementById("buttons").innerHTML = str;

    filter = decodeurl();

    if (typeof filter.from        == 'undefined') filter.from        = document.getElementById("from").value;
    if (typeof filter.titlefilter == 'undefined') filter.titlefilter = document.getElementById("titlefilter").value;
    if (typeof filter.timefilter  == 'undefined') filter.timefilter  = document.getElementById("timefilter").value;
    if (typeof filter.sortfields  == 'undefined') filter.sortfields  = document.getElementById("sortfields").value;
    if (typeof filter.pagesize    == 'undefined') filter.pagesize    = document.getElementById("pagesize").value;
    if (typeof filter.page        == 'undefined') filter.page        = 0;
    if (typeof filter.expanded    == 'undefined') filter.expanded    = -1;

    dvbrequest(filter);
}

function updatestats()
{
    var str = '';

    if (typeof stats.suffix != 'undefined') {
        graphsuffix = stats.suffix;
    }

    str += '<p style="text-align:center"><a href="/dvbgraphs/graph-1week.' + graphsuffix + '" target=_blank><img src="/dvbgraphs/graph-preview.' + graphsuffix + '" style="max-width:240px"></a>';
    str += '</p>' + "\n";
    str += '<p style="text-align:center">';
    str += '<a href="/dvbgraphs/graph-1week.' + graphsuffix + '" target=_blank>1 Week</a>&nbsp;&nbsp;&nbsp;';
    str += '<a href="/dvbgraphs/graph-6months.' + graphsuffix + '" target=_blank>6 Months</a>&nbsp;&nbsp;&nbsp;';
    str += '<a href="/dvbgraphs/graph-all.' + graphsuffix + '" target=_blank>All</a>' + "\n";
    str += '</p>' + "\n";

    if (stats != null) {
        str += 'Recording Rate (programmes per day):<br>' + "\n";
        str += '<table class="stats">'
        if (typeof stats.lastweek != 'undefined') {
            str += '<tr><td style="text-align:left">Last week</td><td>' + Number(stats.lastweek.rate).toFixed(2) + '</td></tr>' + "\n";
        }
        if (typeof stats.last4weeks != 'undefined') {
            str += '<tr><td style="text-align:left">Last 4 weeks</td><td>' + Number(stats.last4weeks.rate).toFixed(2) + '</td></tr>' + "\n";
        }
        if (typeof stats.last6months != 'undefined') {
            str += '<tr><td style="text-align:left">Last 6 months</td><td>' + Number(stats.last6months.rate).toFixed(2) + '</td></tr>' + "\n";
        }
        if (typeof stats.scheduled != 'undefined') {
            str += '<tr><td style="text-align:left">Scheduled</td><td>' + Number(stats.scheduled.rate).toFixed(2) + '</td></tr>' + "\n";
        }
        str += '</table>';
    }

    document.getElementById("stats").innerHTML = str;
}

function getusername(user)
{
    return (user == '') ? defaultuser : user;
}

function limitstring(str)
{
    if (str.length > 40) return str.substring(0, 40) + '...';

    return str;
}

function imdb(str, style)
{
    if (typeof style == 'undefined') style = '';

    return ' (<a href="http://www.imdb.com/find?s=all&q=%22' + str + '%22" target=_blank title="Look up ' + str.replace(/"/g, '&quot;') + ' on IMDB" ' + style + '>imdb</a>)';
}

function findfilter(titlefilter, str, title, style, desc = null)
{
    if ((title == null) || (typeof title == 'undefined')) title = 'Search using filter ' + filter.replace(/"/g, '&quot;');
    if ((style == null) || (typeof style == 'undefined')) style = '';

    titlefilter = titlefilter.replace(/"/g, '\\&quot;');

    return '<a href="javascript:void(0);" onclick="dvbrequest({titlefilter:&quot;' + titlefilter + '&quot;});" ' + style + ' title="' + title + ((desc != null) ? "\n\n" + desc.replace(/"/g, '&quot;') : "") + '">' + str + '</a>';
}

function find(field, str, title, style, desc = null)
{
    if ((title == null) || (typeof title == 'undefined')) title = 'Search for ' + field + ' being ' + str.replace(/"/g, '&quot;');
    if ((style == null) || (typeof style == 'undefined')) style = '';

    return findfilter(field + '="' + str + '"', str, title, style, desc);
}

function findfromfilter(from, titlefilter, timefilter, str, title, style, desc = null)
{
    if ((title      == null) || (typeof title      == 'undefined')) title      = 'Search ' + from + ' using filter ' + titlefilter.replace(/"/g, '&quot;');
    if ((style      == null) || (typeof style      == 'undefined')) style      = '';
    if ((timefilter == null) || (typeof timefilter == 'undefined')) timefilter = '';

    titlefilter = titlefilter.replace(/"/g, '\\&quot;');
    timefilter  = timefilter.replace(/"/g, '\\&quot;');

    return ('<a href="javascript:void(0);" onclick="dvbrequest({' +
            'from:&quot;'        + from        + '&quot;,' +
            'titlefilter:&quot;' + titlefilter + '&quot;,' +
            'timefilter:&quot;'  + timefilter  + '&quot;});" ' +
            style + ' title="' + title + ((desc != null) ? "\n\n" + desc.replace(/"/g, '&quot;') : "") + '">' + str + '</a>');
}

function showstatus(type)
{
    var status = 'Showing ' + (response.from + 1) + ' to ' + (response.from + response.for) + ' of ' + response.total + ' ' + type;

    if (response.for != response.total) {
        var page    = response.page;
        var maxpage = ((response.total + response.pagesize - 1) / response.pagesize) | 0;
        var flag    = false;

        status += ': ';

        if (maxpage > 0) {
            var i;

            if (page > 0) {
                status += ' <a href="javascript:void(0);" onclick="dvbrequest({page:0})">First</a>';

                status += ' <a href="javascript:void(0);" onclick="dvbrequest({page:' + (page - 1) + '})">Previous</a>';
            }

            for (i = 0; i < maxpage; i++) {
                if (!i || (i == (maxpage - 1)) ||
                    ((i < (page + 2)) && ((i + 2) > page))) {
                    status += ' <a href="javascript:void(0);" onclick="dvbrequest({page:' + i + '})">';
                    if (i == page) status += '<b>' + (i + 1) + '</b>';
                    else           status += (i + 1);
                    status += '</a>';
                    flag = true;
                }
                else if (flag) {
                    status += " ... ";
                    flag = false;
                }
            }

            if (page < (maxpage - 1)) {
                status += ' <a href="javascript:void(0);" onclick="dvbrequest({page:' + (page + 1) + '})">Next</a>';

                status += ' <a href="javascript:void(0);" onclick="dvbrequest({page:' + (maxpage - 1) + '})">Last</a>';
            }

            status += '&nbsp;<input type="text" id="page" name="page" value="' + (page + 1) + '" size=4 />&nbsp;<button onclick="gotopage();">Go</button>';
        }
    }

    return status;
}

function gotopage()
{
    if (document.getElementById("page") != null) page = (document.getElementById("page").value | 0) - 1;
    else                                         page = 0;

    dvbrequest({page:page});
}

function calctime(length)
{
    length = ((length + 999) / 1000) | 0;
    return ((length / 60) | 0) + 'm ' + ((length % 60) | 0) + 's';
}

function addcarddetails(prog)
{
    var str = '';

    if (typeof prog.dvbcard != 'undefined') {
        str += ' using DVB card ' + prog.dvbcard;
    }
    if (typeof prog.dvbchannel != 'undefined') {
        str += ', DVB channel \'' + prog.dvbchannel + '\'';
    }
    if (typeof prog.jobid != 'undefined') {
        str += ' (job ' + prog.jobid + ')';
    }

    return str;
}

function addtimesdata(prog)
{
    var str = '';

    if ((typeof prog.actstart != 'undefined') && (typeof prog.actstop != 'undefined') && (prog.actstop > prog.actstart)) {
        if (str != '') str += '<br><br>';
        str += 'Recorded as \'' + prog.filename + '\' ';
        str += ' (' + prog.actstartdate + ' ' + prog.actstarttime + ' - ' + prog.actstoptime + ': ' + calctime(prog.actstop - prog.actstart);
        if (typeof prog.filesize != 'undefined') str += ', filesize ' + ((prog.filesize / (1024 * 1024)) | 0) + 'MB';

        str += ')';
        str += addcarddetails(prog);
        if (typeof prog.videoerrorrate != 'undefined') {
            str += ' (video error rate ' + prog.videoerrorrate.toFixed(2) + ' errors/min)';
        }
        if (typeof prog.convertedfilename != 'undefined') str += '.<br>' + "\n" + 'Will be converted to \'' + prog.convertedfilename + '\'.';

        if (typeof prog.exists != 'undefined') {
            str += '<br>' + "\n";
            if (typeof prog.videopath!= 'undefined') str += '<a href="/videos' + prog.videopath + '" download>';
            str += 'Programme <b>' + (prog.exists ? 'Available' : 'Unavailable') + '</b>.';
            if (typeof prog.videopath!= 'undefined') str += '</a>';
        }

        if ((typeof prog.flags.ignorerecording != 'undefined') && prog.flags.ignorerecording) {
            str += ' (Programme <b>ignored</b> during scheduling)';
        }
    }
    else if ((typeof prog.recstart != 'undefined') && (typeof prog.recstop != 'undefined') && (prog.recstop > prog.recstart)) {
        if (str != '') str += '<br><br>';
        str += 'To be recorded as \'' + prog.filename + '\' ';
        str += '(' + prog.recstartdate + ' ' + prog.recstarttime + ' - ' + prog.recstoptime + ': ' + calctime(prog.recstop - prog.recstart) + ')';

        str += addcarddetails(prog);

        if (typeof prog.convertedfilename != 'undefined') str += '.<br>' + "\n" + 'Will be converted to \'' + prog.convertedfilename + '\'.';
    }

    return str;
}

function populateusers()
{
    var str = '';

    if (typeof response.users != 'undefined') {
        str += '<table class="users">';
        str += '<tr><th colspan=4 style="text-align: left">&nbsp;Default User Folders</th></tr>';
        str += '<tr><th>User</th><th>Folder</th><th>Freespace</th><th>Search</th></tr>';

        for (i = 0; i < response.users.length; i++) {
            var user     = response.users[i];
            var username = getusername(user.user);
            var level    = user.level + 0;

            if (level > 2) level = 2;

            str += '<tr class="diskspacelevel' + level + '"><td>' + username + '</td><td style="text-align:left;">' + user.fullfolder + '</td>';

            if (typeof user.freespace != 'undefined') {
                str += '<td>' + (Math.round(user.freespace * 100.0) / 100.0) + 'G</td>';
            }
            else str += '<td>Unknown</td>';

            str += '<td>';
            if ((username == defaultuser) || (username == '')) {
                str += findfromfilter('Combined', '', defaulttimefilter, 'Combined', 'Search for recordings in combined listings for the default user') + '&nbsp;';
                str += findfromfilter('Requested', '', '', 'Requested', 'Search for requested recordings for the default user') + '&nbsp;';
                str += findfromfilter('Recorded', '', '', 'Recorded', 'Search for recordings for the default user') + '&nbsp;';
                str += findfromfilter('Patterns', 'user=""', '', 'Patterns', 'Display scheduling patterns for the default user') + '&nbsp;';
            }
            else {
                str += findfromfilter('Combined', 'user="' + username + '"', defaulttimefilter, 'Combined', 'Search for recordings in combined listings for user ' + username) + '&nbsp;';
                str += findfromfilter('Requested', 'user="' + username + '"', '', 'Requested', 'Search for requested recordings for user ' + username) + '&nbsp;';
                str += findfromfilter('Recorded', 'user="' + username + '"', '', 'Recorded', 'Search for recorded programmes for user ' + username) + '&nbsp;';
                str += findfromfilter('Patterns', 'user="' + username + '"', '', 'Patterns', 'Display scheduling patterns for user ' + username) + '&nbsp;';
            }
            str += '</td>';
            str += '</tr>';
        }

        if (typeof response.diskspace != 'undefined') {
            str += '<tr><th colspan=4 style="text-align: left">&nbsp;Folders Used By Scheduled Programmes</th></tr>';
            str += '<tr><th>User</th><th>Folder</th><th>Freespace</th><th>Search</th></tr>';

            for (i = 0; i < response.diskspace.length; i++) {
                var diskspace  = response.diskspace[i];
                var username   = getusername(diskspace.user);
                var level      = diskspace.level + 0;

                if (level > 2) level = 2;

                str += '<tr class="diskspacelevel' + level + '"><td>' + username + '</td><td style="text-align:left;">' + diskspace.fullfolder + '</td>';

                if (typeof diskspace.freespace != 'undefined') {
                    str += '<td>' + (Math.round(diskspace.freespace * 100.0) / 100.0) + 'G';
                }
                else str += '<td>Unknown';

                str += '</td>';

                str += '<td>';
                str += findfromfilter('Combined', 'filename@<"' + diskspace.fullfolder + '"', defaulttimefilter, 'Combined', 'Search for recordings in combined listings with folder ' + diskspace.folder) + '&nbsp;';
                str += findfromfilter('Requested', 'filename@<"' + diskspace.fullfolder + '"', '', 'Requested', 'Search for requested recordings with folder ' + diskspace.folder) + '&nbsp;';
                str += findfromfilter('Recorded', 'filename@<"' + diskspace.fullfolder + '"', '', 'Recorded', 'Search for recorded programmes with folder ' + diskspace.folder) + '&nbsp;';
                str += findfromfilter('Patterns', 'filename@<"' + diskspace.fullfolder + '"', '', 'Patterns', 'Display scheduling patterns with folder ' + diskspace.folder) + '&nbsp;';
                str += '</td>';

                str += '</tr>';
            }
        }

        str += '</table>';
    }
    else str += '<h2>No user information</h2>';

    document.getElementById("users").innerHTML = str;
}

function strpad(num, len)
{
    var numstr = num.toString();
    return ('0000' + numstr).slice(-Math.max(len, numstr.length));
}

function populatedates(prog)
{
    var locale      = 'en-GB';
    var timeoptions = {timeZone: 'UTC', hour: '2-digit', minute: '2-digit'};
    var dateoptions = {timeZone: 'UTC', weekday: 'short', year: 'numeric', month: 'short', day: '2-digit'};

    if ((typeof prog.start != 'undefined') &&
        (typeof prog.stop  != 'undefined')) {
        var startdate = new Date(prog.start);
        var stopdate  = new Date(prog.stop);
        prog.starttime    = startdate.toLocaleString(locale, timeoptions);
        prog.stoptime     = stopdate.toLocaleString(locale, timeoptions);
        prog.startdate    = startdate.toLocaleString(locale, dateoptions).replace(/, /g, '#').replace(/ /g, '-').replace(/#/g, ' ');
    }

    if ((typeof prog.recstart != 'undefined') &&
        (typeof prog.recstop  != 'undefined')) {
        var startdate = new Date(prog.recstart);
        var stopdate  = new Date(prog.recstop);
        prog.recstarttime    = startdate.toLocaleString(locale, timeoptions);
        prog.recstoptime     = stopdate.toLocaleString(locale, timeoptions);
        prog.recstartdate    = startdate.toLocaleString(locale, dateoptions).replace(/, /g, '#').replace(/ /g, '-').replace(/#/g, ' ');
    }

    if ((typeof prog.actstart != 'undefined') &&
        (typeof prog.actstop  != 'undefined')) {
        var startdate = new Date(prog.actstart);
        var stopdate  = new Date(prog.actstop);
        prog.actstarttime    = startdate.toLocaleString(locale, timeoptions);
        prog.actstoptime     = stopdate.toLocaleString(locale, timeoptions);
        prog.actstartdate    = startdate.toLocaleString(locale, dateoptions).replace(/, /g, '#').replace(/ /g, '-').replace(/#/g, ' ');
    }
}

function gettimesearchstring(datems)
{
    return (new Date(datems)).toISOString();
}

function adddownloadlink(prog)
{
    var str = '';

    str += '<a href="' + prog.videopath + '" download title="Download ' + prog.title + ' to computer">Download</a>';
    str += '&nbsp;&nbsp;&nbsp;<a href="' + '/dvb/video.php?prog=' + encodeURIComponent(prog.base64) + '" title="Watch ' + prog.title + ' in browser" target=_blank>Watch</a>';
    if ((typeof prog.subfiles != 'undefined') && (prog.subfiles.length > 0)) {
        var i;

        str += '<br>(Sub files:';
        for (i = 0; i < prog.subfiles.length; i++) {
            str += '&nbsp;<a href="' + prog.subfiles[i] + '" download title="Download ' + prog.title + ' sub file ' + (i + 1) + ' to computer">' + (i + 1) + '</a>';
        }
        str += ')';
    }

    if (isAndroid) {
        var i, progname = prog.title;

        if (typeof prog.subtitle != 'undefined') {
            progname += ' / ' + prog.subtitle;
        }

        for (i = 0; i < videointents.length; i++) {
            var intent = videointents[i];

            str += '<br><a href="intent:' + window.location.origin + prog.videopath;
            str += '#Intent;package=' + intent.package + ';';
            if (typeof intent.action != 'undefined') {
                str += 'action=' + intent.action + ';';
            }
            if (typeof intent.type != 'undefined') {
                str += 'type=' + intent.type + ';';
            }
            str += 'S.title=' + encodeURIComponent(progname) + ';end"';
            str += ' title="Watch ' + progname + ' in ' + intent.name + '">Watch in ' + videointents[i].name + '</a>';
        }
    }

    return str;
}

function addarchivedownloadlink(prog)
{
    var str = '';

    str += '<a href="' + prog.videoarchivepath + '" download title="Download ' + prog.title + ' (archive) to computer">Download Archive</a>';

    return str;
}

function populateprogs(id)
{
    var astr = '';
    var i;

    displayfilter(0, -1, '');

    if ((typeof response.progs != 'undefined') && (response.for > 0)) {
        var verbosity = document.getElementById("verbosity").value;
        var status = showstatus('programmes');

        document.getElementById("status").innerHTML = status;
        document.getElementById("statusbottom").innerHTML = status;

        if (response.progs.length > 0) {
            var changed = false;

            astr += '<table class="proglist">';

            patterns = [];
            patterns[0] = searchpattern;

            for (i = 0; i < response.progs.length; i++) {
                if (typeof response.progs[i].starttime == 'undefined') {
                    populatedates(response.progs[i]);

                    if (typeof response.progs[i].recorded != 'undefined') populatedates(response.progs[i].recorded);
                    if (typeof response.progs[i].scheduled != 'undefined') populatedates(response.progs[i].scheduled);
                }

                var prog     = response.progs[i];
                var selected = (i == id);

                if ((typeof prog.html == 'undefined') || (selected != prog.html.selected)) {
                    var prog1      = prog;
                    var headerstr  = '';
                    var detailsstr = '';
                    var classname  = '';
                    var trstr      = '';

                    if      (typeof prog.recorded  != 'undefined') prog1 = prog.recorded;
                    else if (typeof prog.scheduled != 'undefined') prog1 = prog.scheduled;

                    if      (prog.flags.postprocessing)           classname = ' class="processing"';
                    else if (prog.flags.running)                  classname = ' class="recording"';
                    else if (prog.flags.failed)                   classname = ' class="failed"';
                    else if (prog.flags.scheduled)                classname = ' class="scheduled"';
                    else if (prog.flags.highvideoerrorrate)       classname = ' class="highvideoerrorrate"';
                    else if (prog1.flags.recorded)                classname = ' class="recorded"';
                    else if (prog1.flags.rejected)                classname = ' class="rejected"';
                    else if (prog1.flags.scheduled)               classname = ' class="scheduled"';
                    else if (prog.flags.film || prog1.flags.film) classname = ' class="film"';

                    trstr += '<tr' + classname + ' id="prog' + i + 'header"></tr>';
                    trstr += '<tr' + classname + ' id="prog' + i + 'details"></tr>';

                    headerstr += '<td style="width:20px;cursor:pointer;" onclick="dvbrequest({expanded:' + (selected ? -1 : i) + '});"><img src="' + (selected ? 'close.png' : 'open.png') + '" />';
                    headerstr += '<td>';
                    headerstr += find('start', prog.startdate, 'Search for programmes on this day');
                    headerstr += '</td><td>';
                    headerstr += findfilter('stop>"' + gettimesearchstring(prog.start) + '" start<"' + gettimesearchstring(prog.stop) + '"', prog.starttime + ' - ' + prog.stoptime, 'Search for programmes during these times');
                    if (showchannelicons) {
                        headerstr += '</td><td>';
                        if (typeof prog.channelicon != 'undefined') headerstr += '<a href="' + prog.channelicon + '"><img src="' + prog.channelicon + '" style="' + iconimgstyle + '" /></a>';
                        else headerstr += '&nbsp;';
                    }
                    headerstr += '</td><td>';
                    headerstr += find('channel', prog.channel, 'Search for programmes on this channel');

                    headerstr += '</td><td>';
                    headerstr += '<span style="font-size:90%;">';
                    var str1 = '';
                    if (typeof prog.episode != 'undefined') {
                        var str2 = '';

                        if (typeof prog.episode.series != 'undefined') {
                            str2 += 'S' + prog.episode.series;
                        }
                        if (typeof prog.episode.episode != 'undefined') {
                            str2 += 'E' + strpad(prog.episode.episode, 2);
                        }
                        if (typeof prog.episode.episodes != 'undefined') {
                            str2 += 'T' + strpad(prog.episode.episodes, 2);
                        }

                        if (typeof prog.episode.series != 'undefined') {
                            str1 += findfromfilter('Combined', 'title="' + prog.title + '" series=' + prog.episode.series, '', str2, 'View series ' + prog.episode.series + ' scheduled/recorded of this programme');
                        }
                        else str1 += str2;
                    }

                    if ((typeof prog.brandseriesepisode != 'undefined') && (prog.brandseriesepisode.full != '')) {
                        var str2 = '';

                        if (prog.brandseriesepisode.brand != '') str2 += findfromfilter('Combined', 'brandseriesepisode~"' + prog.brandseriesepisode.brand + '.*.*"', '', prog.brandseriesepisode.brand, 'View programmes of this brand in scheduled/recorded programmes');
                        if (prog.brandseriesepisode.series != '') {
                            if (str2 != '') str2 += '&nbsp;';
                            str2 += findfromfilter('Combined', 'brandseriesepisode~"' + prog.brandseriesepisode.brand + '.' + prog.brandseriesepisode.series + '.*"', '', prog.brandseriesepisode.series, 'View programmes of this series in scheduled/recorded programmes');
                        }
                        if (prog.brandseriesepisode.episode != '') {
                            if (str2 != '') str2 += '&nbsp;';
                            str2 += findfromfilter('Combined', 'brandseriesepisode~"' + prog.brandseriesepisode.brand + '.' + prog.brandseriesepisode.series + '.' + prog.brandseriesepisode.episode + '"', '', prog.brandseriesepisode.episode, 'View programmes of this episode in scheduled/recorded programmes');
                        }

                        if (str1 != '') str1 += '<br>';
                        str1 += str2;
                    }

                    if ((typeof prog.episodeid != 'undefined') && (prog.episodeid != '')) {
                        if (str1 != '') str1 += '<br>';
                        str1 += findfromfilter('Combined', 'episodeid="' + prog.episodeid + '"', '', prog.episodeid, 'View programmes of this ID in scheduled/recorded programmes');
                    }

                    if (str1 != '') headerstr += str1;
                    else            headerstr += '<br>&nbsp;'

                    headerstr += '</span>';

                    headerstr += '</td><td>';
                    if (typeof prog.user != 'undefined') {
                        headerstr += find('user', getusername(prog.user), 'Seach for programmes assigned to this user');
                    }
                    else if (typeof prog1.user != 'undefined') {
                        headerstr += find('user', getusername(prog1.user), 'Seach for programmes assigned to this user');
                    }
                    else headerstr += '&nbsp;';

                    if (showprogicons) {
                        headerstr += '</td><td>';
                        if (typeof prog.icon != 'undefined') headerstr += '<a href="' + prog.icon + '"><img src="' + prog.icon + '" style="' + iconimgstyle + '"/></a>';
                        else headerstr += '&nbsp;';
                    }

                    var downloadlink = '';
                    if (prog.flags.postprocessing || prog.flags.running) ;
                    else if ((typeof prog.recorded != 'undefined') &&
                             (typeof prog.recorded.videopath!= 'undefined')) {
                        downloadlink += adddownloadlink(prog.recorded);
                    }
                    else if (typeof prog.videopath != 'undefined') {
                        downloadlink += adddownloadlink(prog);
                    }
                    if ((typeof prog.recorded != 'undefined') &&
                        (typeof prog.recorded.videoarchivepath!= 'undefined')) {
                        if (downloadlink != '') downloadlink += '&nbsp;&nbsp;&nbsp;';
                        downloadlink += addarchivedownloadlink(prog.recorded);
                    }
                    else if (typeof prog.videoarchivepath != 'undefined') {
                        if (downloadlink != '') downloadlink += '&nbsp;&nbsp;&nbsp;';
                        downloadlink += addarchivedownloadlink(prog);
                    }

                    headerstr += '</td><td class="title"';
                    if (downloadlink == '') headerstr += ' colspan=2';
                    headerstr += '>';
                    headerstr += find('title', prog.title, null, null, prog.desc) + imdb(prog.title);

                    if (typeof prog.subtitle != 'undefined') {
                        headerstr += ' / ' + find('subtitle', prog.subtitle) + imdb(prog.subtitle);
                    }

                    headerstr += '{reltime}';
                    headerstr += '</td>';

                    if (downloadlink != '') headerstr += '<td style="font-size:90%;">' + downloadlink + '</td>';

                    headerstr += '<td>';
                    headerstr += '<td style="width:20px;cursor:pointer;" onclick="dvbrequest({expanded:' + (selected ? -1 : i) + '});"><img src="' + (selected ? 'close.png' : 'open.png') + '" />';
                    headerstr += '</td></tr>';

                    var progvb = selected ? 10 : verbosity;
                    if (progvb > 1) {
                        detailsstr += '<td class="desc" colspan=12>';

                        if (prog.flags.postprocessing) {
                            detailsstr += '<span style="font-size:150%;">-- Post Processing Now --</span><br><br>';
                        }
                        if (prog.flags.running) {
                            detailsstr += '<span style="font-size:150%;">-- Recording Now --';

                            var now = new Date().getTime();
                            if (now < prog.recstop) detailsstr += '&nbsp;&nbsp;<progress value="' + ((now - prog.recstart) / 1000) + '" max="' + ((prog.recstop - prog.recstart) / 1000) + '"></progress>';

                            detailsstr += '</span><br><br>';
                        }
                        if (prog.flags.highvideoerrorrate) {
                            detailsstr += '<span style="font-size:150%;">-- High Video Error Rate --</span><br><br>';
                        }
                        if (prog.flags.recordfailed) {
                            detailsstr += '<span style="font-size:150%;">-- Recording Failed --</span><br><br>';
                        }
                        if (prog.flags.postprocessfailed) {
                            detailsstr += '<span style="font-size:150%;">-- Post-Processing Failed --</span><br><br>';
                        }
                        if (prog.flags.conversionfailed) {
                            detailsstr += '<span style="font-size:150%;">-- Conversion Failed --</span><br><br>';
                        }
                        if (prog.flags.failed) {
                            detailsstr += '<span style="font-size:150%;">-- Failed --</span><br><br>';
                        }
                        if (prog.flags.rejected) {
                            detailsstr += '<span style="font-size:150%;">-- Rejected --</span><br><br>';
                        }

                        if ((progvb > 3) &&
                            !prog.flags.recorded &&
                            !prog.flags.scheduled &&
                            !prog.flags.rejected &&
                            !prog.flags.failed &&
                            !prog.flags.recordable) detailsstr += '<span style="font-size:150%;">-- Not Recordable --</span><br><br>';

                        if ((progvb > 1) && (typeof prog.icon != 'undefined')) {
                            detailsstr += '<table class="proglist" style="border:0px"><tr' + classname + '><td style="border:0px">';
                            detailsstr += '<a href="' + prog.icon + '">';
                            detailsstr += '<img src="' + prog.icon + '" style="' + iconimgbigstyle + '" />';
                            detailsstr += '</a>';
                            detailsstr += '</td><td class="desc" style="width:100%">';
                        }

                        if ((progvb > 1) && (typeof prog.desc != 'undefined')) {
                            detailsstr += prog.desc;
                        }

                        if ((progvb > 1) && (typeof prog.icon != 'undefined')) {
                            detailsstr += '</td></tr></table>';
                        }

                        str1 = '';
                        if ((progvb > 2) && (typeof prog.category != 'undefined')) {
                            if (str1 != '') str1 += ' ';
                            str1 += find('category', prog.category);
                            if (typeof prog.subcategory != 'undefined') {
                                str1 += ' (';

                                var j;
                                for (j = 0; j < prog.subcategory.length; j++) {
                                    if (j) str1 += ', ';
                                    str1 += find('subcategory', prog.subcategory[j]);
                                }
                                str1 += ')';
                            }
                            str1 += '.';
                        }

                        if ((progvb > 2) && (typeof prog.rating != 'undefined')) {
                            if (str1 != '') str1 += ' ';
                            str1 += find('rating', prog.rating) + '.';
                        }

                        if ((progvb > 2) && (typeof prog.director != 'undefined')) {
                            if (str1 != '') str1 += ' ';
                            str1 += 'Directed by ' + find('director', prog.director) + imdb(prog.director) + '.';
                        }

                        if ((progvb > 2) && (typeof prog.year != 'undefined')) {
                            if (str1 != '') str1 += ' ';
                            str1 += 'Made in ' + find('year', '' + prog.year) + '.';
                        }

                        if ((progvb > 2) && (typeof prog.episode != 'undefined')) {
                            var str2 = '';

                            if (typeof prog.episode.series != 'undefined') {
                                str2 += 'Series ' + prog.episode.series;
                            }

                            if (typeof prog.episode.episode != 'undefined') {
                                if (str2 != '') str2 += ', episode ';
                                else            str2 += 'Episode ';
                                str2 += prog.episode.episode;
                                if (typeof prog.episode.episodes != 'undefined') {
                                    str2 += ' of ' + prog.episode.episodes;
                                }
                            }

                            if (str2 != '') str2 += '.';

                            if (str1 != '') str1 += ' ';
                            str1 += str2;
                        }
                        else if ((progvb > 2) && (typeof prog.assignedepisode != 'undefined')) {
                            var str2 = '';

                            str2 += 'Assigned episode ' + prog.assignedepisode + '.';

                            if (str1 != '') str1 += ' ';
                            str1 += str2;
                        }

                        if ((progvb > 2) && ((typeof prog.actors != 'undefined') && (prog.actors.length > 0))) {
                            var str2 = '';

                            for (j = 0; j < prog.actors.length; j++) {
                                if (str2 != '') str2 += ', ';
                                str2 += find('actor', prog.actors[j]) + imdb(prog.actors[j]);
                            }

                            if (str1 != '') str1 += ' ';
                            str1 += 'Starring ' + str2 + '.';
                        }

                        if ((progvb > 2) && (typeof prog.dvbchannel != 'undefined')) {
                            str1 += ' DVB channel ' + find('dvbchannel', prog.dvbchannel) + '.';
                        }

                        if (str1 != '') {
                            detailsstr += '<br><br>';
                            detailsstr += str1;
                        }

                        str1 = '';

                        if ((progvb > 3) &&
                            !prog.flags.recorded       &&
                            !prog.flags.scheduled      &&
                            !prog.flags.rejected       &&
                            !prog.flags.failed         &&
                            !prog.flags.running        &&
                            !prog.flags.postprocessing &&
                            prog.flags.recordable      &&
                            (typeof prog.recorded  == 'undefined') &&
                            (typeof prog.scheduled == 'undefined')) {
                            //str1 += '<br><br>';
                            if (typeof response.users != 'undefined') {
                                str1 += '<select class="addrecord" id="addrec' + i + 'user">';
                                for (j = 0; j < response.users.length; j++) {
                                    var user = getusername(response.users[j].user);

                                    str1 += '<option>' + user + '</option>';
                                }
                                str1 += '</select>';
                            }
                            else str1 += defaultuser;

                            str1 += '&nbsp;&nbsp;&nbsp;';
                            if (typeof prog.flags.film != 'undefined' && prog.flags.film) {
                                str1 += '<button class="addrecord" onclick="recordprogramme(' + i + ')">Record Film</button>';
                            }
                            else {
                                str1 += '<button class="addrecord" onclick="recordprogramme(' + i + ')">Record Programme</button>';
                                str1 += '&nbsp;&nbsp;&nbsp;';
                                str1 += '<button class="addrecord" onclick="recordseries(' + i + ')">Record Series</button>';
                            }
                            str1 += '<br><br>';
                        }

                        if ((progvb > 3) &&
                            (typeof prog.episode != 'undefined')) {
                            var j, k, n = 1, series = (typeof prog.episode.series != 'undefined');

                            if (series) {
                                str1 += 'Series:';
                                if (typeof prog.series != 'undefined') str1 += '<br>';
                                else str1 += '&nbsp;&nbsp;';

                                str1 += findfromfilter('Combined', 'title="' + prog.title + '" series>=1', '', 'All', 'View all series scheduled/recorded of this programme');
                                if (typeof prog.series != 'undefined') str1 += '<br>';
                            }
                            else str1 += 'All episodes:<br>';

                            if (series) n = prog.episode.series + 1;
                            if ((typeof prog.series != 'undefined') && (prog.series.length > n)) n = prog.series.length;

                            var dec = 3;
                            if (n > 10)  dec++;
                            if (n > 100) dec++;

                            for (j = 0; j < n; j++) {
                                if (typeof prog.series == 'undefined') str1 += '&nbsp;&nbsp;&nbsp;';

                                if ((typeof prog.series    != 'undefined') &&
                                    (typeof prog.series[j] != 'undefined') &&
                                    (prog.series[j].state != 'empty')) {
                                    var valid = 0, str3 = '';

                                    for (k = 0; k < prog.series[j].episodes.length; k++) {
                                        var pattern, alttext;

                                        if ((k % 100) == 0) {
                                            var en1 = k + 1, en2 = Math.min(en1 + 99, prog.series[j].episodes.length);

                                            if (j == 0) {
                                                str3 += findfromfilter('Combined', 'title="' + prog.title + '" episode>=' + en1 + ' episode<=' + en2, '', 'E' + strpad(en1, dec, '0') + ' to E' + strpad(en2, dec, '0'), 'View episode range scheduled/recorded of this programme');
                                            }
                                            else {
                                                var dec2 = (prog.series[j].episodes.length >= 100) ? 3 : 2;
                                                str3 += findfromfilter('Combined', 'title="' + prog.title + '" series=' + j + ' episode>=' + en1 + ' episode<=' + en2, '', 'S' + strpad(j, 2) + ' E' + strpad(en1, dec2) + ' to E' + strpad(en2, dec2), 'View series ' + j + ' scheduled/recorded of this programme');
                                            }

                                            str3 += ': <span class="episodelist">';
                                        }

                                        pattern = '';
                                        alttext = '';
                                        if (j > 0) {
                                            pattern = 'series=' + j + ' ';
                                            alttext = 'Series ' + j + ' ';
                                        }

                                        pattern += 'episode=' + (k + 1);
                                        alttext += 'Episode ' + (k + 1);

                                        valid++;

                                        if      (prog.series[j].episodes[k] == 'r') alttext += ' (Recorded)';
                                        else if (prog.series[j].episodes[k] == 'a') alttext += ' (Available)';
                                        else if (prog.series[j].episodes[k] == 's') alttext += ' (Scheduled)';
                                        else if (prog.series[j].episodes[k] == 'x') alttext += ' (Rejected)';
                                        else if (prog.series[j].episodes[k] == '-') {
                                            alttext += ' (Unknown)';
                                            valid--;
                                        }

                                        str3 += findfromfilter('Combined', 'title="' + prog.title + '" ' + pattern, '', prog.series[j].episodes[k], alttext);

                                        if ((k % 10) == 9) str3 += ' ';

                                        if (((k % 100) == 99) || (k == (prog.series[j].episodes.length - 1))) {
                                            if (valid > 0) {
                                                str3 += '</span><br>';
                                                str1 += str3;
                                            }

                                            str3 = '';
                                            valid = 0;
                                        }
                                    }
                                }
                            }

                            if (typeof prog.series == 'undefined') str1 += '<br>';

                            str1 += '<br>';
                        }

                        if ((progvb > 3) && (typeof prog1.pattern != 'undefined')) {
                            if (str1 != '') str1 += ' ';

                            str1 += 'Found using filter \'' + limitstring(prog1.pattern) + '\'';
                            if (typeof prog1.user != 'undefined') {
                                str1 += ' by user \'' + getusername(prog1.user) + '\'';
                            }
                            if (typeof prog1.pri != 'undefined') {
                                str1 += ', priority ' + prog1.pri;
                            }
                            if ((typeof prog1.score != 'undefined') && (prog1.score > 0)) {
                                str1 += ' (score ' + prog1.score + ')';
                            }
                            str1 += ':';

                            if ((typeof prog1.patternparsed != 'undefined') &&
                                (prog1.patternparsed != '')) {
                                str1 += '<table class="patternlist">';
                                str1 += '<tr><th>Status</th><th>Priority</th><th>User</th><th class="desc">Pattern</th></tr>';
                                str1 += populatepattern(prog1.patternparsed, i + 1);
                                str1 += '</table>';
                            }
                        }

                        var str2 = '';
                        if (typeof prog.recorded != 'undefined') {
                            str2 = findfromfilter('Recorded', 'uuid="' + prog.recorded.uuid + '"', '', 'Recorded', 'Go direct to recorded version', 'class="progsearch"') + ': ' + addtimesdata(prog.recorded);
                        }
                        else if ((typeof prog.scheduled != 'undefined') && (prog.scheduled.uuid != prog.uuid)) {
                            str2 = findfromfilter('Scheduled', 'uuid="' + prog.scheduled.uuid + '"', '', 'Scheduled', 'Go direct to scheduled version', 'class="progsearch"') + ': ' + addtimesdata(prog.scheduled);
                        }
                        else str2 = addtimesdata(prog1);

                        if (str2 != '') {
                            if (str1 != '') str1 += '<br><br>';

                            str1 += str2;
                        }

                        if (str1 != '') str1 += '<br><br>';

                        str1 += '<table class="progsearch"><tr><td>Programme</td><td>';
                        str1 += findfromfilter('Listings', 'prog="' + prog.base64 + '"', '', 'Listings', 'Search for occurrences of this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Combined', 'prog="' + prog.base64 + '"', '', 'Combined', 'Search for scheduled/recorded occurrences of this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Requested', 'prog="' + prog.base64 + '"', '', 'Requested', 'Search for requested recordings of this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Recorded', 'prog="' + prog.base64 + '"', '', 'Recorded', 'Search for recordiongs of this programme');
                        str1 += '</td></tr>';
                        str1 += '<tr><td>Title</td><td>';
                        str1 += findfromfilter('Listings', 'title="' + prog.title + '"', '', 'Listings', 'Search for occurrences of programmes with this title');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Combined', 'title="' + prog.title + '"', '', 'Combined', 'Search for scheduled/recorded occurrences of programmes with this title');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Requested', 'title="' + prog.title + '"', '', 'Requested', 'Search for requested recordings of programmes with this title');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Recorded', 'title="' + prog.title + '"', '', 'Recorded', 'Search for recorded programmes with this title');
                        str1 += '</td></tr>';
                        if (typeof prog.subtitle != 'undefined') {
                            str1 += '<tr><td>Title / Subtitle</td><td>';
                            str1 += findfromfilter('Listings', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Listings', 'Search for occurrences of programmes with this title and subtitle');
                            str1 += '</td><td>';
                            str1 += findfromfilter('Combined', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Combined', 'Search for scheduled/recorded occurrences of programmes with this title and subtitle');
                            str1 += '</td><td>';
                            str1 += findfromfilter('Requested', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Requested', 'Search for requested recordings of programmes with this title and subtitle');
                            str1 += '</td><td>';
                            str1 += findfromfilter('Recorded', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Recorded', 'Search for recorded programmes with this title and subtitle');
                            str1 += '</td></tr>';
                        }
                        str1 += '<tr><td>Clashes</td><td>';
                        str1 += findfromfilter('Listings',
                                               'uuid!="' + prog.uuid + '" ' + 'stop>"' + gettimesearchstring(prog.start) + '" ' + 'start<"' + gettimesearchstring(prog.stop) + '"',
                                               '',
                                               'Listings',
                                               'Search for clashes in listings with this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Combined',
                                               'uuid!="' + prog.uuid + '" ' + 'stop>"' + gettimesearchstring(prog.start) + '" ' + 'start<"' + gettimesearchstring(prog.stop) + '"',
                                               '',
                                               'Combined',
                                               'Search for clashes in scheduled/recorded programmes with this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Requested',
                                               'uuid!="' + prog.uuid + '" ' + 'stop>"' + gettimesearchstring(prog.start) + '" ' + 'start<"' + gettimesearchstring(prog.stop) + '"',
                                               '',
                                               'Requested',
                                               'Search for clashes in requested programmes with this programme');
                        str1 += '</td><td>';
                        str1 += findfromfilter('Recorded',
                                               'uuid!="' + prog.uuid + '" ' + 'stop>"' + gettimesearchstring(prog.start) + '" ' + 'start<"' + gettimesearchstring(prog.stop) + '"',
                                               '',
                                               'Recorded',
                                               'Search for clashes in recorded programmes with this programme');
                        str1 += '</td></tr></table>';

                        if ((progvb > 3) && (prog.flags.recorded || prog.flags.failed) &&
                            ((globals != null) && (typeof globals.candelete != 'undefined') && globals.candelete)) {
                            str1 += '<br>';
                            if ((typeof prog.flags.exists != 'undefined') && prog.flags.exists) {
                                str1 += '<button class="delrecord" onclick="deletevideo(' + i + ')">Delete Video</button>';
                                str1 += '&nbsp;&nbsp;&nbsp;';
                            }
                            str1 += '<button class="delrecord" onclick="deleteprogramme(' + i + ')">Delete Programme</button>';
                        }

                        if (str1 != '') {
                            detailsstr += '<br><br>';
                            detailsstr += str1;
                        }

                        detailsstr += '</td>';
                    }

                    changed |= (document.getElementById("prog" + i + "header") == null);

                    if (typeof response.progs[i].html == 'undefined') response.progs[i].html = {};
                    response.progs[i].html.selected  = selected;
                    response.progs[i].html.headerstr = headerstr;

                    if      (typeof response.progs[i].html.trstr == 'undefined') changed = true;
                    else if (trstr != response.progs[i].html.trstr)              changed = true;
                    response.progs[i].html.trstr = trstr;

                    response.progs[i].html.detailschanged = false;
                    if (document.getElementById("prog" + i + "details") == null) {
                        response.progs[i].html.detailschanged = true;
                    }
                    else if (typeof response.progs[i].html.detailsstr == 'undefined') {
                        response.progs[i].html.detailschanged = true;
                    }
                    else if (detailsstr != response.progs[i].html.detailsstr) {
                        response.progs[i].html.detailschanged = true;
                    }
                    response.progs[i].html.detailsstr = detailsstr;
                }
                else response.progs[i].html.detailschanged = false;

                astr += response.progs[i].html.trstr + "\n";

                var reltimestr = '';
                if (typeof prog.start != 'undefined') {
                    var dt = new Date();
                    var now = dt.getTime() - dt.getTimezoneOffset() * 60000;
                    var offset = now - prog.start;
                    var type = 'Start';
                    var str, str1 = '', str2 = '';

                    if (offset >= 0) {
                        offset = now - prog.stop;
                        type = 'End';
                    }

                    str = ' <span style="font-size:80%;">(';
                    if (offset < 0) {
                        str += type + 's in ';
                        offset = -offset;
                    }
                    else {
                        str += type + 'ed ';
                        str2 = ' ago';
                    }

                    var minutes = ((offset + 59999) / 60000) | 0;
                    var hours   = (minutes / 60) | 0;
                    minutes -= hours * 60;
                    var days    = (hours / 24) | 0;
                    hours -= days * 24;

                    var n = 0;
                    if (days > 0) {
                        if (str1 != '') str1 += ' ';
                        str1 += days + 'd';
                        n++;
                    }
                    if (hours > 0) {
                        if (str1 != '') str1 += ' ';
                        str1 += hours + 'h';
                        n++;
                    }
                    if ((minutes > 0) && (n < 2)) {
                        if (str1 != '') str1 += ' ';
                        str1 += minutes + 'm';
                    }

                    str += str1 + str2 + ')</span>';

                    reltimestr = str;
                }

                var headerstr2 = response.progs[i].html.headerstr.replace('{reltime}', reltimestr);

                response.progs[i].html.headerchanged = false;
                if (document.getElementById("prog" + i + "header") == null) {
                    response.progs[i].html.headerchanged = true;
                }
                else if (typeof response.progs[i].html.headerstr2 == 'undefined') {
                    response.progs[i].html.headerchanged = true;
                }
                else if (headerstr2 != response.progs[i].html.headerstr2) {
                    response.progs[i].html.headerchanged = true;
                }
                response.progs[i].html.headerstr2 = headerstr2;
            }

            astr += '</table>';

            if (changed || (astr != proglistelement)) {
                document.getElementById("list").innerHTML = astr;
                proglistelement = astr;
            }

            for (i = 0; i < response.progs.length; i++) {
                if (response.progs[i].html.headerchanged) {
                    document.getElementById("prog" + i + "header").innerHTML = response.progs[i].html.headerstr2;
                }
                if (response.progs[i].html.detailschanged) {
                    document.getElementById("prog" + i + "details").innerHTML = response.progs[i].html.detailsstr;
                }
            }
        }
    }
    else {
        document.getElementById("status").innerHTML = "No programmes";
        document.getElementById("list").innerHTML = '';
    }
}

function populatetitles(id)
{
    var str = '';

    displayfilter(0, -1, '');

    if ((typeof response.titles != 'undefined') && (response.for > 0)) {
        var status = showstatus('titles');
        var pattern = '';
        var from = 'Combined';
        var p1, p2;

        if (((p1 = currentfilter.from.search("\\(")) >= 0) &&
            ((p2 = currentfilter.from.search("\\)", p1)) >= 0)) {
            from = currentfilter.from.substr(p1 + 1, p2 - p1 - 1);
        }

        proglistelement = '';

        document.getElementById("status").innerHTML = status;
        document.getElementById("statusbottom").innerHTML = status;

        if (typeof response.parsedpattern.pattern != 'undefined') pattern = response.parsedpattern.pattern + ' ';

        if (response.titles.length > 0) {
            var i;

            str += '<table class="titlelist">';

            for (i = 0; i < response.titles.length; i++) {
                var title = response.titles[i];
                var desc  = "\n\n" + title.desc.replace(/"/g, '&quot;');

                str += '<tr';

                if ((title.isfilm > 0) && (title.notfilm == 0)) str += ' class="film"';

                str += '><td style="text-align:left;">';
                str +=          findfromfilter(from, 'title="' + title.title + '"', '', title.title, 'Find all versions of this title' + desc) + '</td>';
                str += '<td>' + findfromfilter(from, pattern + 'title="' + title.title + '"', '', title.total + ' In Total', 'Find all versions of this title') + '</td>';
                str += '<td>' + findfromfilter('Combined', pattern + 'title="' + title.title + '" recorded', '', title.recorded + ' Recorded', 'Find recorded versions of this title') + '</td>';
                str += '<td>' + findfromfilter('Combined', pattern + 'title="' + title.title + '" available', '', title.available + ' Available', 'Find recorded versions of this title that are available') + '</td>';
                str += '<td>' + findfromfilter('Combined', pattern + 'title="' + title.title + '" scheduled', '', title.scheduled + ' Scheduled', 'Find scheduled versions of this title') + '</td>';
                str += '<td>' + findfromfilter('Combined', pattern + 'title="' + title.title + '" failed', '', title.failed + ' Failed', 'Find failed versions of this title') + '</td>';

                str += '</tr>';
            }

            str += '</table>';
        }
    }
    else {
        document.getElementById("status").innerHTML = "No titles";
        document.getElementById("list").innerHTML = '';
    }

    document.getElementById("list").innerHTML = str;
}

function recordprogramme(id)
{
    if (typeof response.progs[id] != 'undefined') {
        var user = document.getElementById('addrec' + id + 'user').value;
        var prog = response.progs[id];
        var pattern = 'title="' + prog.title + '"';
        var postdata = '';

        if (user == defaultuser) user = '';

        if ((typeof prog.flags.film != 'undefined') && prog.flags.film)  pattern += ' film=1 onceonly:=1 pri:=-3';

        if (typeof prog.subtitle != 'undefined') pattern += ' subtitle="' + prog.subtitle + '"';

        postdata += "editpattern=add\n";
        postdata += "newuser=" + user + "\n";
        postdata += "newpattern=" + pattern + "\n";

        if (confirm("Re-schedule after adding programme?")) {
            postdata += "schedule=commit\n";
        }

        dvbrequest({from:"Combined", titlefilter:pattern, timefilter:defaulttimefilter}, postdata);
    }
}

function recordseries(id)
{
    if (typeof response.progs[id] != 'undefined') {
        var user = document.getElementById('addrec' + id + 'user').value;
        var prog = response.progs[id];
        var pattern = 'title="' + prog.title + '"';
        var postdata = '';

        if (user == defaultuser) user = '';

        postdata += "editpattern=add\n";
        postdata += "newuser=" + user + "\n";
        postdata += "newpattern=" + pattern + "\n";

        if (confirm("Re-schedule after adding programme?")) {
            postdata += "schedule=commit\n";
        }

        dvbrequest({from:"Combined", titlefilter:pattern, timefilter:defaulttimefilter}, postdata);
    }
}

function deletevideo(id)
{
    if (typeof response.progs[id] != 'undefined') {
        var prog = response.progs[id];
        var postdata = '';

        postdata += "deleteprogramme=" + prog.uuid + "\n";
        postdata += "type=video\n";

        var progname = prog.title;
        if (typeof prog.subtitle != 'undefined') {
            progname += ' / ' + prog.subtitle;
        }

        if (confirm("Delete video for '" + progname + "'?")) {
            dvbrequest({expanded:-1}, postdata);
        }
    }
}

function deleteprogramme(id)
{
    if (typeof response.progs[id] != 'undefined') {
        var prog = response.progs[id];
        var postdata = '';

        postdata += "deleteprogramme=" + prog.uuid + "\n";
        postdata += "type=recordlist\n";

        var progname = prog.title;
        if (typeof prog.subtitle != 'undefined') {
            progname += ' / ' + prog.subtitle;
        }

        if (confirm("Delete '" + progname + "' from list of recordings?")) {
            if (confirm("Are you really sure you want to delete '" + progname + "' from list of recordings?")) {
                dvbrequest({expanded:-1}, postdata);
            }
        }
    }
}

function addrecfromlisting(id)
{
    if (typeof response.progs[id] != 'undefined') {
        var user = document.getElementById('addrec' + id + 'user').value;
        var prog = response.progs[id];
        var pattern = 'title="' + prog.title + '"';
        var postdata = '';

        if (user == defaultuser) user = '';

        postdata += "editpattern=add\n";
        postdata += "newuser=" + user + "\n";
        postdata += "newpattern=" + pattern + "\n";

        dvbrequest({from:"Patterns", titlefilter:"", timefilter:""}, postdata);
    }
}

function displayfilter(patternid, termindex, type)
{
    var pattern = patterns[patternid];
    var str = '';
    var i, firstset = true;
    var always = false;

    if ((editingpattern >= 0) && (editingpattern != patternid)) {
        displayfilter(editingpattern, -1, '');
        editingpattern = -1;
    }

    if (pattern.terms.length > 0) {
        if (pattern.terms[0].assign) str += 'set&nbsp;';
        else                         str += 'if&nbsp;';
    }

    for (i = 0; i < pattern.terms.length; i++) {
        var term = pattern.terms[i];
        var classname = term.assign ? 'patternset' : 'patternterm';

        if (termindex == i) editingpattern = patternid;

        //if (i) str += '&nbsp;';

        str += '<span class="' + classname + '" id="pattern' + patternid + 'term' + i + 'field" title="Search Field"';
        if (always || ((i == termindex) && (type == 'field'))) {
            str += '><select id="pattern' + patternid + 'term' + i + 'fieldvalue" onchange="displayfilter_changed(' + patternid + ', ' + i + ', &quot;field&quot;)">';
            str += getvalidfields(term.field);
            str += '</select>';
        }
        else str += ' onclick="displayfilter(' + patternid + ', ' + i + ', &quot;field&quot;)">' + patterndefs.fields[term.field].name.replace(/"/g, '&quot;');
        str += '</span>';

        str += '<span class="' + classname + '" id="pattern' + patternid + 'term' + i + 'operator" title="Search Operator"';
        if (always || ((i == termindex) && (type == 'operator'))) {
            str += '><select id="pattern' + patternid + 'term' + i + 'operatorvalue" onchange="displayfilter_changed(' + patternid + ', ' + i + ', &quot;operator&quot;)">';
            str += getvalidoperators(term.field, term.opindex);
            str += '</select>';
        }
        else str += ' onclick="displayfilter(' + patternid + ', ' + i + ', &quot;operator&quot;)">' + patterndefs.operators[term.opindex].text.replace(/"/g, '&quot;');
        str += '</span>';

        str += '<span class="' + classname + '" id="pattern' + patternid + 'term' + i + 'value" title="Search Comparison Value"';
        if (always || ((i == termindex) && (type == 'value'))) {
            str += '><input type="text" id="pattern' + patternid + 'term' + i + 'valuevalue" value="' + term.value.replace(/"/g, '&quot;') + '" onfocusout="displayfilter_changed(' + patternid + ', ' + i + ', &quot;value&quot;)" />';
        }
        else {
            str += ' onclick="displayfilter(' + patternid + ', ' + i + ', &quot;value&quot;)">';
            if (term.value.indexOf(" ") >= 0) str += '"';
            str += term.value;
            if (term.value.indexOf(" ") >= 0) str += '"';
        }
        str += '</span>';

        if (!pattern.terms[i].assign && (i < (pattern.terms.length - 1)) && !pattern.terms[i + 1].assign) {
            str += '<span class="' + classname + '" id="pattern' + patternid + 'term' + i + 'orflag" title="Search And/Or"';
            if (always || ((i == termindex) && (type == 'orflag'))) {
                str += '><select id="pattern' + patternid + 'term' + i + 'orflagvalue" onchange="displayfilter_changed(' + patternid + ', ' + i + ', &quot;orflag&quot;)">';
                str += getvalidorflags(term.orflag);
                str += '</select>';
            }
            else str += ' onclick="displayfilter(' + patternid + ', ' + i + ', &quot;orflag&quot;)">' + patterndefs.orflags[term.orflag].text.replace(/"/g, '&quot;');
            str += '</span>';
        }
        else if (firstset && (i < (pattern.terms.length - 1))) {
            str += "&nbsp;then&nbsp;";
            firstset = false;
        }
        else if (i < (pattern.terms.length - 1)) str += "&nbsp;and&nbsp;";
    }
    str += '<br><br>';

    var obj;
    if ((obj = document.getElementById('pattern' + patternid + 'terms')) != null) {
        obj.innerHTML = str;
    }

    return str;
}

function displayfilter_changed(patternid, termindex, type)
{
    if (type == 'field') {
        var i, field = document.getElementById('pattern' + patternid + 'term' + termindex + type + 'value').value | 0, opindex = patterns[patternid].terms[termindex].opindex;
        var fielddata = patterndefs.fields[field];

        for (i = 0; i < fielddata.operators.length; i++) {
            if (fielddata.operators[i] == opindex) break;
        }

        if (i == fielddata.operators.length) {
            var optext = patterndefs.operators[opindex].text;

            for (i = 0; i < fielddata.operators.length; i++) {
                var j = fielddata.operators[i];
                if (patterndefs.operators[j].text == optext) {
                    opindex = j;
                    break;
                }
            }

            if (i == fielddata.operators.length) opindex = fielddata.operators[0];
        }

        patterns[patternid].terms[termindex].field   = field;
        patterns[patternid].terms[termindex].opindex = opindex;
    }
    else if (type == 'operator') {
        patterns[patternid].terms[termindex].opindex = document.getElementById('pattern' + patternid + 'term' + termindex + type + 'value').value | 0;
    }
    else if (type == 'value') {
        patterns[patternid].terms[termindex].value = document.getElementById('pattern' + patternid + 'term' + termindex + type + 'value').value;
    }
    else if (type == 'orflag') {
        patterns[patternid].terms[termindex].orflag = document.getElementById('pattern' + patternid + 'term' + termindex + type + 'value').value | 0;
    }

    var i, pattern = patterns[patternid].enabled ? '' : '#';
    for (i = 0; i < patterns[patternid].terms.length; i++) {
        if (i) pattern += ' ';
        pattern += patterndefs.fields[patterns[patternid].terms[i].field].name;
        pattern += patterndefs.operators[patterns[patternid].terms[i].opindex].text;
        if (patterns[patternid].terms[i].value.indexOf(" ") >= 0) pattern += '"';
        pattern += patterns[patternid].terms[i].value;
        if (patterns[patternid].terms[i].value.indexOf(" ") >= 0) pattern += '"';
        if (patterns[patternid].terms[i].orflag) pattern += ' or';
    }

    var obj;
    if ((obj = document.getElementById('pattern' + patternid + 'pattern')) != null) {
        obj.value = pattern;
        patternchanged(patternid);
    }
    else displayfilter(patternid, -1, '');
}

function getvalidfields(term_field)
{
    var str = '', k;

    for (k = 0; k < patterndefs.fields.length; k++) {
        var field = patterndefs.fields[k];

        str += '<option';
        if (k == term_field) str += ' selected';
        str += ' value=' + k + ' title="' + field.desc.replace(/"/g, '&quot;') + '">' + field.name.replace(/"/g, '&quot;') + '</option>';
    }

    return str;
}

function getvalidoperators(term_field, term_opindex)
{
    var operators = patterndefs.fields[term_field].operators;
    var str = '', k;
    var map = [];

    for (k = 0; k < operators.length; k++) {
        var opindex  = operators[k];
        var operator = patterndefs.operators[opindex];
        var opcode   = operator.opcode;

        if (typeof map[opcode] == 'undefined') {
            map[opcode] = true;

            str += '<option';
            if (opindex == term_opindex) str += ' selected';
            str += ' value=' + opindex + ' title="' + operator.desc.replace(/"/g, '&quot;') + '">' + operator.text.replace(/"/g, '&quot;') + '</option>';
        }
    }

    return str;
}

function getvalidorflags(term_orflag)
{
    var str = '', k;

    for (k = 0; k < patterndefs.orflags.length; k++) {
        var orflag = patterndefs.orflags[k];

        str += '<option';
        if (k == term_orflag) str += ' selected';
        str += ' value=' + k + ' title="' + orflag.desc.replace(/"/g, '&quot;') + '">' + orflag.text.replace(/"/g, '&quot;') + '</option>';
    }

    return str;
}

function populatepattern(pattern, index)
{
    var str = '', classname = '';
    var j, k;

    if (typeof index == 'undefined') index = patterns.length;

    patterns[index] = pattern;

    if ((typeof pattern.errors != 'undefined') &&
        (pattern.errors != '')) classname = 'error';
    else if (pattern.enabled) classname = 'enabled';
    else                      classname = 'disabled';
    str += '<tr class="' + classname + '"><td>';
    if ((typeof pattern.errors != 'undefined') && (pattern.errors != '')) {
        str += '<div title="Errors: ' + pattern.errors + '"><b>Error</b></div>';
    }
    else if (pattern.enabled) str += 'Enabled';
    else                      str += 'Disabled';

    str += '</td><td>';
    str += '<select id="pattern' + index + 'pri" onchange="patternprichanged(' + index + ')">';
    for (j = 10; j >= -10; j--) {
        str += '<option';
        if (j == pattern.pri) str += ' selected';
        str += '>' + j + '</option>';
    }
    str += '</select>';
    str += '</td><td>';
    if (typeof response.users != 'undefined') {
        str += '<select id="pattern' + index + 'user" onchange="patternchanged(' + index + ')">';
        for (j = 0; j < response.users.length; j++) {
            var user = response.users[j].user;

            str += '<option';
            if (user == pattern.user) str += ' selected';
            str += '>';
            if (user != '') str += user;
            else            str += defaultuser;
            str += '</option>';
        }
    }
    else {
        if (pattern.user != '') str += pattern.user;
        else                    str += defaultuser;
    }

    str += '</td><td class="desc">';
    //str += '<span id="pattern' + index + 'terms">' + displayfilter(index, -1, '') + '</span>';
    if (typeof pattern.addrec != 'undefined') {
        str += '<input type="text" id="newrecpattern" value="' + pattern.pattern.replace(/"/g, '&quot;') + '" onfocusout="addrecchanged(&quot;newrec&quot;)"/>';
    }
    else {
        str += '<input type="text" id="pattern' + index + 'pattern" value="' + pattern.pattern.replace(/"/g, '&quot;') + '" onfocusout="patternchanged(' + index + ')"/>';
    }

    str += '</td></tr>';

    str += '<tr class="' + classname + '"><td colspan=4 class="actions">';
    if ((typeof pattern.errors != 'undefined') && (pattern.errors != '')) {
        str += '<div class="finderrors">' + pattern.errors + '</div>'
        str += '<br>';
    }
    if (typeof pattern.addrec != 'undefined') {
        str += '<button id="newreclink" onclick="addpattern(&quot;newrec&quot;)"><b>Add Pattern</b></button>&nbsp;';
    }
    else {
        str += '<button id="updatepattern' + index + 'link" onclick="updatepattern(' + index + ')">Update</button>&nbsp;';
    }
    if (pattern.enabled) str += '<button onclick="disablepattern(' + index + ')">Disable</button>&nbsp;';
    else                 str += '<button onclick="enablepattern(' + index + ')">Enable</button>&nbsp;';
    str += '<button onclick="deletepattern(' + index + ')">Delete</button>&nbsp;';
    str += 'Search:&nbsp;';
    str += '<button onclick="findusingpattern(&quot;Listings&quot;,&quot;pattern' + index + '&quot;);" title="Find this pattern in listings">Listings</button>&nbsp;';
    str += '<button onclick="findusingpattern(&quot;Combined&quot;,&quot;pattern' + index + '&quot;);" title="Find this pattern in combined listings">Combined</button>&nbsp;';
    str += '<button onclick="findusingpattern(&quot;Requested&quot;,&quot;pattern' + index + '&quot;);" title="Find this pattern in requested programmes">Requested</button>&nbsp;';
    str += '</td></tr>';

    return str;
}

function populatepatterns(id)
{
    var str = '';

    if ((typeof response.patterns != 'undefined') && (response.for > 0)) {
        var status = showstatus('patterns');
        var i, j;

        document.getElementById("status").innerHTML = status;
        document.getElementById("statusbottom").innerHTML = status;

        if (response.patterns.length > 0) {
            str += '<table class="patternlist">';
            str += '<tr><th>Status</th><th>Priority</th><th>User</th><th class="desc">Pattern</th></tr>';

            str += '<tr class="newrec"><td>New</td><td>&nbsp;</td>';
            str += '<td>';
            if (typeof response.users != 'undefined') {
                str += '<select id="newrecuser" onchange="addrecchanged(&quot;newrec&quot;)">';
                for (j = 0; j < response.users.length; j++) {
                    var user = response.users[j].user;

                    str += '<option>';
                    if (user != '') str += user;
                    else            str += defaultuser;
                    str += '</option>';
                }
            }
            else {
                if (pattern.user != '') str += pattern.user;
                else                    str += defaultuser;
            }
            str += '</td><td class="desc">';
            str += '<input type="text" id="newrecpattern" size=50 maxlength=10000 value="" onfocusout="addrecchanged(&quot;newrec&quot;)"/>';
            str += '</td></tr>';
            str += '<tr class="newrec">';
            str += '<td colspan=4 class="actions">';
            str += '<button id="newreclink" onclick="addpattern(&quot;newrec&quot;)">Add Pattern</button>&nbsp;';
            str += 'Search:&nbsp;';
            str += '<button onclick="findusingpattern(&quot;Listings&quot;,&quot;newrec&quot;);" title="Find this pattern in listings">Listings</button>&nbsp;';
            str += '<button onclick="findusingpattern(&quot;Combined&quot;,&quot;newrec&quot;);" title="Find this pattern in combined list">Combined</button>&nbsp;';
            str += '<button onclick="findusingpattern(&quot;Requested&quot;,&quot;newrec&quot;);" title="Find this pattern in requested programmes">Requested</button>&nbsp;';
            str += '</td></tr>';

            patterns = [];
            patterns[0] = searchpattern;
            for (i = 0; i < response.patterns.length; i++) {
                str += populatepattern(response.patterns[i]);
            }
            str += '</table>';
        }
    }
    else {
        document.getElementById("status").innerHTML = "No patterns";
    }

    document.getElementById("list").innerHTML = str;
}

function patternprichanged(index)
{
    if (typeof patterns[index] != 'undefined') {
        var user = patterns[index].user;

        if (user == defaultuser) user = '';

        parserequest(index, user, patterns[index].pattern + " pri:=" + (document.getElementById("pattern" + index + "pri").value * 1));
    }
}

function patternchanged(index)
{
    if (typeof patterns[index] != 'undefined') {
        var updated = ((document.getElementById("pattern" + index + "pattern").value != patterns[index].pattern) ||
                       (document.getElementById("pattern" + index + "user").value    != patterns[index].user));
        if (updated) document.getElementById("updatepattern" + index + "link").innerHTML = "<b>Update</b>";
        else         document.getElementById("updatepattern" + index + "link").innerHTML = "Update";
        displayfilter(index, -1, '');
    }
}

function updatepattern(index)
{
    if (typeof patterns[index] != 'undefined') {
        if ((document.getElementById("pattern" + index + "pattern").value != patterns[index].pattern) ||
            (document.getElementById("pattern" + index + "user").value    != patterns[index].user)) {
            var postdata = "";
            var user    = document.getElementById("pattern" + index + "user").value;
            var pattern = document.getElementById("pattern" + index + "pattern").value;

            if (user == defaultuser) user = '';

            if (pattern != '') {
                postdata += "editpattern=update\n";
                postdata += "user=" + patterns[index].user + "\n";
                postdata += "pattern=" + patterns[index].pattern + "\n";
                postdata += "newuser=" + user + "\n";
                postdata += "newpattern=" + pattern + "\n";
            }

            dvbrequest(currentfilter, postdata);
        }
    }
}

function enablepattern(index)
{
    if (typeof patterns[index] != 'undefined') {
        if (!patterns[index].enabled) {
            var postdata = "";

            postdata += "editpattern=enable\n";
            postdata += "user=" + patterns[index].user + "\n";
            postdata += "pattern=" + patterns[index].pattern + "\n";

            dvbrequest(currentfilter, postdata);
        }
    }
}

function disablepattern(index)
{
    if (typeof patterns[index] != 'undefined') {
        if (patterns[index].enabled) {
            var postdata = "";

            postdata += "editpattern=disable\n";
            postdata += "user=" + patterns[index].user + "\n";
            postdata += "pattern=" + patterns[index].pattern + "\n";

            dvbrequest(currentfilter, postdata);
        }
    }
}

function deletepattern(index)
{
    if (typeof patterns[index] != 'undefined') {
        if (confirm("Delete pattern:\n\n'" + patterns[index].pattern + "'?")) {
            var postdata = "";

            postdata += "editpattern=delete\n";
            postdata += "user=" + patterns[index].user + "\n";
            postdata += "pattern=" + patterns[index].pattern + "\n";

            dvbrequest(currentfilter, postdata);
        }
    }
}

function addrecchanged(srcobject)
{
    if (document.getElementById(srcobject + 'pattern').value != '') {
        document.getElementById(srcobject + 'link').innerHTML = "<b>Add Pattern</b>";
    }
    else {
        document.getElementById(srcobject + 'link').innerHTML = "Add Pattern";
    }
}

function addpattern(srcobject)
{
    var user    = document.getElementById(srcobject + 'user').value;
    var pattern = document.getElementById(srcobject + 'pattern').value;

    if (user == defaultuser) user = '';

    if (pattern != '') {
        var postdata = "";

        postdata += "editpattern=add\n";
        postdata += "newuser=" + user + "\n";
        postdata += "newpattern=" + pattern + "\n";

        dvbrequest(currentfilter, postdata);
    }
}

function findusingpattern(from, srcobject)
{
    var str = document.getElementById(srcobject + 'pattern').value;

    if (str.substr(0, 1) == '#') str = str.substr(1);

    dvbrequest({from:from, titlefilter:str});
}

function populatelogs(id)
{
    var str = '';
    var i;

    if ((typeof response.loglines != 'undefined') && (response.for > 0)) {
        var status = showstatus('loglines');

        document.getElementById("status").innerHTML = status;
        document.getElementById("statusbottom").innerHTML = status;

        if (response.loglines.length > 0) {
            str += '<table class="loglines">';
            for (i = 0; i < response.loglines.length; i++) {
                str += "<tr><td><code>" + response.loglines[i] + "</code></td></tr>";
            }
            str += '</table>';
        }
    }
    else {
        document.getElementById("status").innerHTML = "No logs";
    }

    document.getElementById("list").innerHTML = str;
}

function populate(id)
{
    if (response != null) {
        if      (typeof response.showchannels != 'undefined') showchannels();
        else if (typeof response.patterns     != 'undefined') populatepatterns(id);
        else if (typeof response.loglines     != 'undefined') populatelogs(id);
        else if (typeof response.titles       != 'undefined') populatetitles(id);
        else if (typeof response.progs        != 'undefined') populateprogs(id);

        populateusers();
    }

    displayfilter(0, -1, '');
}

function dvbrequest(filter, postdata, stackrequest, timerbased)
{
    if (waitingforstreamurlstimer != null) {
        clearTimeout(waitingforstreamurlstimer);
        waitingforstreamurlstimer = null;
    }

    if (typeof filter != 'undefined') {
        if (typeof filter.from        == 'undefined') filter.from        = currentfilter.from;
        else                                          document.getElementById("from").value = filter.from;
        if (typeof filter.titlefilter == 'undefined') filter.titlefilter = currentfilter.titlefilter;
        else                                          document.getElementById("titlefilter").value = filter.titlefilter;
        if (typeof filter.timefilter  == 'undefined') filter.timefilter  = currentfilter.timefilter;
        else                                          document.getElementById("timefilter").value = filter.timefilter;
        if (typeof filter.sortfields  == 'undefined') filter.sortfields  = currentfilter.sortfields;
        else                                          document.getElementById("sortfields").value = filter.sortfields;
        if (typeof filter.page        == 'undefined') {
            if ((filter.from        != currentfilter.from) ||
                (filter.titlefilter != currentfilter.titlefilter) ||
                (filter.timefilter  != currentfilter.timefilter) ||
                (filter.sortfields  != currentfilter.sortfields)) {
                filter.page = 0;
            }
            else filter.page = currentfilter.page;
        }
        if (typeof filter.pagesize    == 'undefined') filter.pagesize    = currentfilter.pagesize;
        else                                          document.getElementById("pagesize").value = filter.pagesize;
        if (typeof filter.expanded    == 'undefined') {
            if ((filter.from        != currentfilter.from) ||
                (filter.titlefilter != currentfilter.titlefilter) ||
                (filter.timefilter  != currentfilter.timefilter) ||
                (filter.sortfields  != currentfilter.sortfields) ||
                (filter.page        != currentfilter.page)) {
                filter.expanded = -1;
            }
            else filter.expanded = currentfilter.expanded;
        }
    }
    else filter = currentfilter;

    filter.page     = filter.page     | 0;
    filter.pagesize = filter.pagesize | 0;

    if ((typeof stackrequest == 'undefined') || stackrequest) {
        //console.log("Pushing " + JSON.stringify(filter) + " on to stack at position " + window.history.length);
        window.history.pushState(filter, "", window.location);
    }

    {
        var title = 'DVB Programmes: ' + generatefilterdescription(filter);
        if (title != document.title) {
            document.title = title;
        }
        //document.getElementById("title").innerHTML = title;
    }

    {
        var link = JSON.stringify(filter);
        document.getElementById("link").innerHTML = '<a href="?' + encodeURIComponent(link) + '">Link</a>';
    }

    if (((currentfilter == null) ||
         (typeof postdata    != 'undefined') ||
         ((typeof filter.fetch != 'undefined') && filter.fetch) ||
         (filter.from        != currentfilter.from) ||
         (filter.titlefilter != currentfilter.titlefilter) ||
         (filter.timefilter  != currentfilter.timefilter) ||
         (filter.sortfields  != currentfilter.sortfields) ||
         (filter.page        != currentfilter.page) ||
         (filter.pagesize    != currentfilter.pagesize)) &&
        !((typeof filter.fetch != 'undefined') && !filter.fetch)) {

        if ((typeof timerbased == 'undefined') || !timerbased) {
            document.getElementById("status").innerHTML = '<span style="font-size:200%;">Fetching...</span>';
        }

        if ((xmlhttp != null) && (xmlhttp.readState < 4)) {
            xmlhttp.abort();
        }

        if (window.XMLHttpRequest) {
            // code for IE7+, Firefox, Chrome, Opera, Safari
            xmlhttp = new XMLHttpRequest();

            xmlhttp.onreadystatechange = function() {
                if (xmlhttp.readyState == 4) {
                    if (xmlhttp.status == 200) {
                        response = JSON.parse(xmlhttp.responseText);

                        if (typeof response.globals != 'undefined') globals = response.globals;

                        var errors = '';
                        if ((typeof response.errors != 'undefined') && (response.errors != '')) {
                            errors = '<br><div class="finderrors">' + response.errors + '</div>';
                        }
                        document.getElementById("finderrors").innerHTML = errors;

                        if (typeof response.stats != 'undefined') {
                            stats = response.stats;
                            updatestats();
                        }

                        if (typeof response.searches != 'undefined') {
                            searches = {
                                ref:response.searchesref,
                                view:[],
                            };

                            var i, str = '';
                            for (i = 0; i < response.searches.length; i++) {
                                var search = response.searches[i];

                                searches.view[i] = search.search;
                                searches.view[i].timefilter = "";
                                searches.view[i].fetch = true;

                                str += '<button onclick="dvbrequest(searches.view[' + i + '])">' + search.title + '</button><br>';
                            }

                            if (str != '') str += '<br>';

                            document.getElementById("usersearches").innerHTML = str;
                        }

                        if (typeof response.channels != 'undefined') {
                            channels = {
                                ref:response.channelsref,
                                channels:response.channels,
                            };
                        }

                        document.getElementById("errormsg").innerHTML = '';
                        if (typeof response.counts != 'undefined') {
                            if ((typeof response.counts.rejected != 'undefined') && (response.counts.rejected > 0)) {
                                document.getElementById("errormsg").innerHTML = '<div class="errormsg">Warning: ' + response.counts.rejected + ' programme' + ((response.counts.rejected != 1) ? 's' : '') + ' not scheduled! <a href="javascript:void(0);" onclick="dvbrequest({from:&quot;Combined&quot;,titlefilter:&quot;rejected=1&quot;,timefilter:&quot;&quot;});">View</a></div>';
                            }
                        }

                        if (typeof response.patterndefs != 'undefined') {
                            patterndefs = response.patterndefs;
                        }

                        if (typeof response.parsedpattern != 'undefined') {
                            searchpattern = response.parsedpattern;
                            patterns[0] = searchpattern;
                        }

                        updatecurrentfilter(filter);

                        populate(filter.expanded);
                    }
                    else document.getElementById("status").innerHTML = "<h1>Server returned error " + xmlhttp.status + "</h1>";
                }
            }
        }

        xmlhttp.open("POST", "dvb.sh", true);

        var data = "";
        data += "from=" + filter.from + "\n";
        data += "filter=" + getfullfilter(filter) + "\n";
        data += "titlefilter=" + filter.titlefilter + "\n";
        data += "timefilter=" + filter.timefilter + "\n";
        data += "sortfields=" + filter.sortfields + "\n";
        data += "page=" + filter.page + "\n";
        data += "pagesize=" + filter.pagesize + "\n";
        if (searches != null) {
            data += "searchesref=" + searches.ref + "\n";
        }
        if (channels != null) {
            data += "channelsref=" + channels.ref + "\n";
        }
        if (stats != null) {
            data += "statsref=" + stats.ref + "\n";
        }
        if (patterndefs == null) {
            data += "patterndefs=\n";
        }
        if (typeof postdata != 'undefined') data += postdata.replace(/\\n/g, "\n");

        xmlhttp.send(data);

        if ((typeof timerbased == 'undefined') || !timerbased) {
            document.getElementById("status").innerHTML += " <button onclick=\"abortfind()\">Abort</button>";
        }
        document.getElementById("statusbottom").innerHTML = '';
    }
    else {
        populate(filter.expanded);

        updatecurrentfilter(filter);
    }
}

function generatefilterdescription(filter)
{
    var str, fullfilter = getfullfilter(filter);
    var title;

    str = 'Page ' + (filter.page + 1) + ' of ' + filter.from;
    if (fullfilter != '') {
        str += '\nFiltered using \'' + limitstring(fullfilter) + '\'';
    }
    if ((filter.expanded >= 0) &&
        (response != null) &&
        (typeof response.progs != 'undefined') &&
        (filter.expanded < response.progs.length)) {
        var prog = response.progs[filter.expanded];
        title = prog.title;

        if (typeof prog.subtitle != 'undefined') title += ' / ' + prog.subtitle;
        if (typeof prog.episode != 'undefined') {
            title += ' (';
            if (typeof prog.episode.series != 'undefined') {
                title += 'S' + prog.episode.series;
            }
            if (typeof prog.episode.episode != 'undefined') {
                title += 'E' + strpad(prog.episode.episode, 2);
            }
            if (typeof prog.episode.episodes != 'undefined') {
                title += 'T' + strpad(prog.episode.episodes, 2);
            }
            title += ')';
        }
        else if (typeof prog.assignedepisode != 'undefined') {
            title += ' (F' + prog.assignedepisode + ')';
        }

        str += '\nViewing \'' + title + '\'';
    }
    filter.longdesc = str;

    str = 'Page ' + (filter.page + 1) + ' of ' + filter.from;
    if (filter.titlefilter != '') str += ' \'' + limitstring(filter.titlefilter) + '\'';
    if ((filter.expanded >= 0) && (typeof title != 'undefined')) {
        str += ' (' + title + ')';
    }
    filter.shortdesc = str;

    return str;
}

function parserequest(index, user, pattern)
{
    if ((xmlhttp != null) && (xmlhttp.readState < 4)) {
        xmlhttp.abort();
    }

    if (window.XMLHttpRequest) {
        // code for IE7+, Firefox, Chrome, Opera, Safari
        xmlhttp = new XMLHttpRequest();

        xmlhttp.onreadystatechange = function() {
            if (xmlhttp.readyState == 4) {
                if (xmlhttp.status == 200) {
                    var parseresp = JSON.parse(xmlhttp.responseText);

                    if (typeof parseresp.newpattern != 'undefined') {
                        var newpattern = parseresp.parsedpattern;

                        document.getElementById("pattern" + index + "pattern").value = newpattern.pattern;
                        patternchanged(index);
                    }
                }
                else document.getElementById("status").innerHTML = "<h1>Server returned error " + xmlhttp.status + "</h1>";
            }
        }
    }

    xmlhttp.open("POST", "dvb.php", true);

    var data = "";
    data += "parse=" + pattern + "\n";
    data += "user="  + user    + "\n";
    xmlhttp.send(data);
}

function getfullfilter(filter)
{
    var str = '';

    if (typeof filter.titlefilter != 'undefined') str += filter.titlefilter;

    if ((typeof filter.timefilter != 'undefined') &&
        (filter.timefilter != '')) {
        if (str != '') str += ' ';
        str += filter.timefilter;
    }

    return str;
}

function updatecurrentfilter(filter)
{
    currentfilter = filter;
}

function decodeurl()
{
    var filter = {};
    var search = location.search.substring(1);

    if ((typeof search != 'undefined') && (search != '')) {
        search = decodeURIComponent(search);
        filter = JSON.parse(search, function(key, value) {return value;});
    }

    return filter;
}

function updatepagesize()
{
    if (response != null) {
        dvbrequest({page:(response.from / (document.getElementById("pagesize").value | 0)),
                    pagesize:document.getElementById("pagesize").value});
    }
    else dvbrequest({pagesize:document.getElementById("pagesize").value});
}

function updatesortfields()
{
    dvbrequest({sortfields:document.getElementById("sortfields").value});
}

function abortfind()
{
    if ((xmlhttp != null) && (xmlhttp.readState < 4)) {
        xmlhttp.abort();

        document.getElementById("status").innerHTML = "Aborted";
    }
    else if (xmlhttp != null) {
        document.getElementById("status").innerHTML = "Cannot abort, state is" + xmlhttp.readState;
    }
}

function reschedule()
{
    if (confirm("Schedule all programmes?")) {
        dvbrequest({from:"Combined",titlefilter:"",timefilter:defaulttimefilter,expanded:-1,fetch:true}, 'schedule=commit');
    }
}

function requeststreams()
{
    dvbrequest({},"showchannels=1", false, true);
}

function addstreamcontrol(type, channel, activestreams)
{
    var str = '';

    if ((typeof channel.dvb != 'undefined') &&
        (typeof channel.dvb.hasvideo != 'undefined') &&
        (typeof channel.dvb.hasaudio != 'undefined') &&
        channel.dvb.hasvideo &&
        channel.dvb.hasaudio) {
        str += '<td>';
        if ((typeof activestreams != 'undefined') &&
            (((typeof channel.dvb.name != 'undefined') &&
              (typeof (stream = activestreams.find(stream => ((stream.type == type) && (stream.name.toLowerCase() == channel.dvb.name.toLowerCase())))) != 'undefined')) ||
             ((typeof channel.dvb.convertedname != 'undefined') &&
              (typeof (stream = activestreams.find(stream => ((stream.type == type) && (stream.name.toLowerCase() == channel.dvb.convertedname.toLowerCase())))) != 'undefined')) ||
             ((typeof channel.xmltv != 'undefined') &&
              (((typeof channel.xmltv.name != 'undefined') &&
                (typeof (stream = activestreams.find(stream => ((stream.type == type) && (stream.name.toLowerCase() == channel.xmltv.name.toLowerCase())))) != 'undefined')) ||
               ((typeof channel.xmltv.convertedname != 'undefined') &&
                (typeof (stream = activestreams.find(stream => ((stream.type == type) && (stream.name.toLowerCase() == channel.xmltv.convertedname.toLowerCase())))) != 'undefined')))))) {
            str += '<a href="javascript:void(0);" onclick="dvbrequest({},&quot;stopstream=' + stream.name + '\\nshowchannels=1&quot;)">Stop</a>';
        }
        else if ((typeof channel.xmltv != 'undefined') &&
                 (typeof channel.xmltv.convertedname != 'undefined')) {
            str += '<a href="javascript:void(0);" onclick="dvbrequest({},&quot;start' + type + 'stream=' + channel.xmltv.convertedname + '\\nshowchannels=1&quot;)">Start</a>';
        }
        else if ((typeof channel.dvb != 'undefined') &&
                 (typeof channel.dvb.convertedname != 'undefined')) {
            str += '<a href="javascript:void(0);" onclick="dvbrequest({},&quot;start' + type + 'stream=' + channel.dvb.convertedname + '\\nshowchannels=1&quot;)">Start</a>';
        }
        else str += '&nbsp;';
        str += '</td>';
    }
    else str += '<td>&nbsp;</td>';

    return str;
}

function showchannels()
{
    var str = '', i, j;
    var validchannels = 0;
    var activestreams = response.activestreams;
    var waitingforstreamurls = false;

    if ((typeof activestreams != 'undefined') &&
        (activestreams.length > 0)) {
        var str = '';

        str += '<table class="streamlist"><tr>';
        str += '<th style="text-align:left">Channel</th><th>Type</th><th>URL</th><th>Stop</th></tr>'

        for (i = 0; i < activestreams.length; i++) {
            var stream = activestreams[i];

            str += '<tr><td style="text-align:left">' + stream.name + '</td>';
            str += '<td>' + stream.type.toUpperCase() + '</td>';
            if (stream.url.length > 0) {
                str += '<td style="text-align:left"><a href="' + stream.url + '" title="Watch ' + stream.name + ' in browser" target=_blank>' + stream.url + '</a></td>';
            }
            else {
                str += '<td>Waiting for stream...</td>';
                waitingforstreamurls = true;
            }

            str += '<td><a href="javascript:void(0);" onclick="dvbrequest({},&quot;stopstream=' + stream.name + '\\nshowchannels=1&quot;)">Stop</a></td>';
            str += '</tr>';
        }

        str += '</table>';
        str += '<h2><a href="javascript:void(0);" onclick="dvbrequest({},&quot;stopstreams=*\\nshowchannels=1&quot;)">Stop All Streams</a></h2>';
        document.getElementById("status").innerHTML = str;
    }

    if (waitingforstreamurls) {
        waitingforstreamurlstimer = setTimeout(requeststreams, 1000)
    }

    str += '<table class="channellist"><tr>';
    str += '<th>LCN</th>';
    str += '<th style="text-align:left">XMLTV Channel</th>';
    str += '<th style="text-align:left">DVB Channel</th>';
    str += '<th>HLS Streaming</th>';
    str += '<th>HTTP Streaming</th>';
    str += '<th>DVB Frequency</th>';
    str += '<th>DVB PID List</th>';
    str += '<th>Type</th>';
    str += '</tr>';

    document.getElementById("statusbottom").innerHTML = '';
    document.getElementById("status").innerHTML = '';

    if (typeof channels != 'undefined') {
        for (i = 0; i < channels.channels.length; i++) {
            var channel = channels.channels[i];
            var valid = false;

            {
                str += '<tr';

                if ((typeof channel.dvb == 'undefined') ||
                    (typeof channel.dvb.lcn == 'undefined') ||
                    (typeof channel.dvb.convertedname == 'undefined') ||
                    (typeof channel.xmltv == 'undefined') ||
                    (typeof channel.xmltv.convertedname == 'undefined') ||
                    (channel.xmltv.convertedname.toLowerCase() != channel.dvb.convertedname.toLowerCase())) {
                    str += ' class="error"';
                }
                else valid = true;

                str += '>';
                if ((typeof channel.dvb != 'undefined') &&
                    (typeof channel.dvb.lcn != 'undefined')) {
                    str += '<td>' + channel.dvb.lcn + '</td>';
                }
                else str += '<td>&nbsp;</td>';

                if (typeof channel.xmltv != 'undefined') {
                    str += '<td style="text-align:left"';
                    if ((typeof channel.xmltv.name != 'undefined') &&
                        ((typeof channel.xmltv.convertedname == 'undefined') ||
                         (channel.xmltv.name != channel.xmltv.convertedname))) {
                        str += ' title="Original: ' + channel.xmltv.name + '"';
                    }
                    str += '>';
                    if (typeof channel.xmltv.convertedname != 'undefined') {
                        str += channel.xmltv.convertedname;
                    }
                    else str += '&nbsp;';
                    str += '</td>';
                }
                else str += '<td>&nbsp;</td>';

                if (typeof channel.dvb != 'undefined') {
                    str += '<td style="text-align:left"';
                    if ((typeof channel.dvb.name != 'undefined') &&
                        ((typeof channel.dvb.convertedname == 'undefined') ||
                         (channel.dvb.name != channel.dvb.convertedname))) {
                        str += ' title="Original: ' + channel.dvb.name + '"';
                    }
                    str += '>';
                    if (typeof channel.dvb.convertedname != 'undefined') {
                        str += channel.dvb.convertedname;
                    }
                    else str += '&nbsp;';
                    str += '</td>';
                }
                else str += '<td>&nbsp;</td>';

                str += addstreamcontrol('hls', channel, activestreams);
                str += addstreamcontrol('http', channel, activestreams);

                if ((typeof channel.dvb != 'undefined') &&
                    (typeof channel.dvb.frequency != 'undefined')) {
                    str += '<td>' + channel.dvb.frequency + '</td>';
                }
                else str += '<td>&nbsp;</td>';

                if ((typeof channel.dvb != 'undefined') &&
                    (typeof channel.dvb.pidlist != 'undefined') &&
                    (channel.dvb.pidlist.length > 0)) {
                    str += '<td>';
                    for (j = 0; j < channel.dvb.pidlist.length; j++) {
                        if (j > 0) str += ', ';
                        str += channel.dvb.pidlist[j];
                    }
                    str += '</td>';
                }
                else str += '<td>&nbsp;</td>';

                if ((typeof channel.dvb != 'undefined') &&
                    (typeof channel.dvb.hasvideo != 'undefined') &&
                    (typeof channel.dvb.hasaudio != 'undefined')) {
                    str += '<td>';
                    if (channel.dvb.hasvideo && channel.dvb.hasaudio) str += "TV";
                    else if (channel.dvb.hasaudio)                    str += "Radio";
                    else                                              str += "&nbsp;";
                    str += '</td>';
                }
                else str += '<td>&nbsp;</td>';

                str += '</tr>';

                if (valid) validchannels++;
            }
        }

        document.getElementById("statusbottom").innerHTML = validchannels + ' valid channels, ' +  channels.channels.length + ' total channels';
    }
    else {
        document.getElementById("statusbottom").innerHTML = 'No channel data';
    }

    str += '</table>';

    document.getElementById("list").innerHTML = str;
}
