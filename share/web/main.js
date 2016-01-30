var defaultuser = 'default';
var defaulttimefilter = 'start>=yesterday,midnight';

var iconimgsize = 'width=57 height=32';
var iconimgbigsize = 'width=171 height=96';
var daynames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
var monthnames = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
var response = null;
var patterns = [];
var editingpattern = -1;
var searchpattern = null;
var patterndefs = null;
var xmlhttp  = null;
var filterlist = {
	index:0,
	current:null,
	list:[]
};
var filters = [
	{text:"Sources:<br>"},
	{
		title:"Listings",
		filter:{from:"Listings",titlefilter:"",timefilter:"start>now",expanded:-1,fetch:true},
	},
	{
		title:"Combined",
		filter:{from:"Combined",titlefilter:"",timefilter:defaulttimefilter,expanded:-1,fetch:true},
	},
	{
		title:"Requested",
		filter:{from:"Requested",titlefilter:"",timefilter:"",expanded:-1,fetch:true},
	},
	{
		title:"Rejected",
		filter:{from:"Combined",titlefilter:"rejected=1",timefilter:defaulttimefilter,expanded:-1,fetch:true},
	},
	{
		title:"Scheduled",
		filter:{from:"Combined",titlefilter:"scheduled=1",timefilter:defaulttimefilter,expanded:-1,fetch:true},
	},
	{
		title:"Recorded",
		filter:{from:"Combined",titlefilter:"recorded=1",timefilter:defaulttimefilter,expanded:-1,fetch:true},
	},
	{
		title:"Failures",
		filter:{from:"Combined",titlefilter:"failed=1",timefilter:defaulttimefilter,expanded:-1,fetch:true},
	},
	{
		title:"Patterns",
		filter:{from:"Patterns",expanded:-1,fetch:true},
	},
	{
		title:"Clear Filter",
		filter:{titlefilter:"",expanded:-1,fetch:true},
	},
	{
		title:"Sky",
		filter:{from:"Sky",titlefilter:"",timefilter:"start>now",expanded:-1,fetch:true},
	},
	{text:"<br>DVB Logs:<br>"},
	{
		title:"Today",
		filter:{from:"Logs",timefilter:"start=today",expanded:-1,fetch:true},
	},
	{
		title:"Yesterday",
		filter:{from:"Logs",timefilter:"start=yesterday",expanded:-1,fetch:true},
	},
];
var searches = null;
var link     = null;
var globals  = null;

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
	if (typeof filter.pagesize    == 'undefined') filter.pagesize    = document.getElementById("pagesize").value;
	if (typeof filter.page        == 'undefined') filter.page        = 0;
	if (typeof filter.expanded    == 'undefined') filter.expanded    = -1;

	dvbrequest(filter);
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

function find(field, str, title, style)
{
	if (typeof title == 'undefined') title = 'Search for ' + field + ' being ' + str.replace(/"/g, '&quot;');
	if (typeof style == 'undefined') style = '';

	return findfilter(field + '="' + str + '"', str, title, style);
}

function findfilter(titlefilter, str, title, style)
{
	if (typeof title == 'undefined') title = 'Search using filter ' + filter.replace(/"/g, '&quot;');
	if (typeof style == 'undefined') style = '';

	titlefilter = titlefilter.replace(/"/g, '\\&quot;');

	return '<a href="javascript:void(0);" onclick="dvbrequest({titlefilter:&quot;' + titlefilter + '&quot;});" ' + style + ' title="' + title + '">' + str + '</a>';
}

function findfromfilter(from, titlefilter, timefilter, str, title, style)
{
	if (typeof title 	  == 'undefined') title = 'Search ' + from + ' using filter ' + filter.replace(/"/g, '&quot;');
	if (typeof style 	  == 'undefined') style = '';
	if (typeof timefilter == 'undefined') timefilter = '';

	titlefilter = titlefilter.replace(/"/g, '\\&quot;');
	timefilter  = timefilter.replace(/"/g, '\\&quot;');

	return ('<a href="javascript:void(0);" onclick="dvbrequest({' +
			'from:&quot;'        + from        + '&quot;,' +
			'titlefilter:&quot;' + titlefilter + '&quot;,' + 
			'timefilter:&quot;'  + timefilter  + '&quot;});" ' +
			style + ' title="' + title + '">' + str + '</a>');
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
					else		   status += (i + 1);
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
	else									     page = 0;

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
		str += 'Recorded as \'' + prog.filename + '\' (' + prog.actstartdate + ' ' + prog.actstarttime + ' - ' + prog.actstoptime + ': ' + calctime(prog.actstop - prog.actstart) + ')';
		if (typeof prog.filesize != 'undefined') str += ' filesize ' + ((prog.filesize / (1024 * 1024)) | 0) + 'MB';

		str += addcarddetails(prog);
		str += '.';

		if (typeof prog.exists != 'undefined') {
			str += ' ';
			if (typeof prog.file != 'undefined') str += '<a href="/videos' + prog.file + '" download>';
			str += 'Programme <b>' + (prog.exists ? 'Available' : 'Unavailable') + '</b>.';
			if (typeof prog.file != 'undefined') str += '</a>';
		}

		if ((typeof prog.flags.ignorerecording != 'undefined') && prog.flags.ignorerecording) {
			str += ' (Programme <b>ignored</b> during scheduling)';
		}
	}
	else if ((typeof prog.recstart != 'undefined') && (typeof prog.recstop != 'undefined') && (prog.recstop > prog.recstart)) {
		if (str != '') str += '<br><br>';
		str += 'To be recorded as \'' + prog.filename + '\' (' + prog.recstartdate + ' ' + prog.recstarttime + ' - ' + prog.recstoptime + ': ' + calctime(prog.recstop - prog.recstart) + ')';

		str += addcarddetails(prog);
		str += '.';
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
				str += '<td>' + user.freespace + '</td>';
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
					str += '<td>' + diskspace.freespace;
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

function getdatestring(datems)
{
	var date = new Date(datems);
	return daynames[date.getDay()] + ' ' + strpad(date.getDate(), 2) + '-' + monthnames[date.getMonth()] + '-' + date.getFullYear();
}

function populatedates(prog)
{
	prog.starttime    = new Date(prog.start).toTimeString().substr(0, 5);
	prog.stoptime     = new Date(prog.stop).toTimeString().substr(0, 5);
	prog.startdate    = getdatestring(prog.start);

	prog.recstarttime = new Date(prog.recstart).toTimeString().substr(0, 5);
	prog.recstoptime  = new Date(prog.recstop).toTimeString().substr(0, 5);
	prog.recstartdate = getdatestring(prog.recstart);

	prog.actstarttime = new Date(prog.actstart).toTimeString().substr(0, 5);
	prog.actstoptime  = new Date(prog.actstop).toTimeString().substr(0, 5);
	prog.actstartdate = getdatestring(prog.actstart);
}

function adddownloadlink(prog)
{
	var str = '';
	
	str += '<a href="' + prog.file + '" download title="Download ' + prog.title + ' to computer">Download</a>';
	str += ' or <a href="video.php?prog=' + encodeURIComponent(prog.base64) + '" title="Watch ' + prog.title + ' in browser" target=_blank>Watch</a>';
	if ((typeof prog.subfiles != 'undefined') && (prog.subfiles.length > 0)) {
		var i;

		str += '<br>(Sub files:';
		for (i = 0; i < prog.subfiles.length; i++) {
			str += '&nbsp;<a href="' + prog.subfiles[i] + '" download title="Download ' + prog.title + ' sub file ' + (i + 1) + ' to computer">' + (i + 1) + '</a>';
		}
		str += ')';
	}
	
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
			astr += '<table class="proglist">';

			patterns = [];
			patterns[0] = searchpattern;

			for (i = 0; i < response.progs.length; i++) {
				if (typeof response.progs[i].starttime == 'undefined') {
					populatedates(response.progs[i]);

					if (typeof response.progs[i].scheduled != 'undefined') populatedates(response.progs[i].scheduled);
					if (typeof response.progs[i].recorded  != 'undefined') populatedates(response.progs[i].recorded);
					if (typeof response.progs[i].rejected  != 'undefined') populatedates(response.progs[i].rejected);
				}

				var prog      = response.progs[i];
				var selected  = (i == id);

				if ((typeof prog.html == 'undefined') || (selected != prog.html.selected)) {
					var prog1     = prog;
					var str       = '';
					var classname = '';

					if		(typeof prog.recorded  != 'undefined') prog1 = prog.recorded;
					else if (typeof prog.scheduled != 'undefined') prog1 = prog.scheduled;
					else if (typeof prog.rejected  != 'undefined') prog1 = prog.rejected;

					if		(prog.flags.postprocessing)											 classname = ' class="processing"';
					else if	(prog.flags.running)												 classname = ' class="recording"';
					else if (prog.flags.failed)													 classname = ' class="failed"';
					else if (prog.flags.scheduled || (typeof prog.scheduled != 'undefined')) 	 classname = ' class="scheduled"';
					else if	(prog.flags.recorded  || (typeof prog.recorded  != 'undefined'))   	 classname = ' class="recorded"';
					else if (prog.flags.rejected  || (typeof prog.rejected  != 'undefined'))	 classname = ' class="rejected"';
					else if ((typeof prog.category != 'undefined') && (prog.category == 'Film')) classname = ' class="film"';

					str += '<tr' + classname + '>';
					str += '<td style="width:20px;cursor:pointer;" onclick="dvbrequest({expanded:' + (selected ? -1 : i) + '});"><img src="' + (selected ? 'close.png' : 'open.png') + '" />';
					str += '<td>';
					str += find('start', prog.startdate, 'Search for programmes on this day');
					str += '</td><td>';
					str += findfilter('stop>"' + (new Date(prog.start)).toISOString() + '" start<"' + (new Date(prog.stop)).toISOString() + '"', prog.starttime + ' - ' + prog.stoptime, 'Search for programmes during these times');
					str += '</td><td>';
					if (typeof prog.channelicon != 'undefined') str += '<a href="' + prog.channelicon + '"><img src="' + prog.channelicon + '" ' + iconimgsize + ' /></a>';
					else										str += '&nbsp;';
					str += '</td><td>';
					str += find('channel', prog.channel, 'Seach for programmes on this channel');

					str += '</td><td>';
					if (typeof prog.episode != 'undefined') {
						var str1 = '';

						if (typeof prog.episode.series != 'undefined') {
							str1 += 'S' + prog.episode.series;
						}
						if (typeof prog.episode.episode != 'undefined') {
							str1 += 'E' + strpad(prog.episode.episode, 2);
						}
						if (typeof prog.episode.episodes != 'undefined') {
							str1 += 'T' + strpad(prog.episode.episodes, 2);
						}

						str += '<span style="font-size:90%;">';
						if (typeof prog.episode.series != 'undefined') {	
							str += findfromfilter('Combined', 'title="' + prog.title + '" series=' + prog.episode.series, '', str1, 'View series ' + prog.episode.series + ' scheduled/recorded of this programme');
						}
						else str += str1;
						str += '</span>';
					}
					else if ((typeof prog.brandseriesepisode != 'undefined') && (prog.brandseriesepisode != '')) {
						str += '<span style="font-size:90%;">F' + prog.brandseriesepisode + '</span>';
					}
					else if ((typeof prog.assignedepisode != 'undefined') && (prog.assignedepisode != '')) {
						str += '<span style="font-size:90%;">F' + prog.assignedepisode + '</span>';
					}
					else str += '&nbsp;';

					str += '</td><td>';
					if (typeof prog1.user != 'undefined') {
						str += find('user', getusername(prog1.user), 'Seach for programmes assigned to this user');
					}
					else str += '&nbsp;';

					str += '</td><td>';
					if (typeof prog.icon != 'undefined') str += '<a href="' + prog.icon + '"><img src="' + prog.icon + '" ' + iconimgsize + ' /></a>';
					else								 str += '&nbsp;';

					str += '</td><td class="title">';
					str += find('title', prog.title) + imdb(prog.title);

					if (typeof prog.subtitle != 'undefined') {
						str += ' / ' + find('subtitle', prog.subtitle) + imdb(prog.subtitle);
					}

					str += '{reltime}';
					str += '</td><td style="font-size:90%;">';
					if (prog.flags.postprocessing || prog.flags.running) str += '&nbsp;';
					else if (typeof prog.file != 'undefined') str += adddownloadlink(prog);
					else if ((typeof prog.recorded != 'undefined') &&
							 (typeof prog.recorded.file != 'undefined')) str += adddownloadlink(prog.recorded);
					else str += '&nbsp;';
					str += '</td><td>';
					str += '<td style="width:20px;cursor:pointer;" onclick="dvbrequest({expanded:' + (selected ? -1 : i) + '});"><img src="' + (selected ? 'close.png' : 'open.png') + '" />';
					str += '</td></tr>';

					var progvb = selected ? 10 : verbosity;
					if (progvb > 1) {
						var str1 = '';

						str += '<tr' + classname + '>';
						str += '<td class="desc" colspan=12>';

						if (prog.flags.postprocessing) {
							str += '<span style="font-size:150%;">-- Post Processing Now --</span><br><br>';
						}
						else if (prog.flags.running) {
							str += '<span style="font-size:150%;">-- Recording Now --</span><br><br>';
						}
						else if (prog.flags.failed) {
							str += '<span style="font-size:150%;">-- Recording Failed --</span><br><br>';
						}
						else if (prog.flags.rejected) {
							str += '<span style="font-size:150%;">-- Rejected --</span><br><br>';
						}

						if ((progvb > 1) && (typeof prog.icon != 'undefined')) {
							str += '<table class="proglist" style="border:0px"><tr' + classname + '><td style="border:0px">';
							str += '<a href="' + prog.icon + '"><img src="' + prog.icon + '" ' + iconimgbigsize + ' /></a>';
							str += '</td><td class="desc" style="width:100%">';
						}

						if ((progvb > 1) && (typeof prog.desc != 'undefined')) {
							str += prog.desc;
						}

						if ((progvb > 1) && (typeof prog.icon != 'undefined')) {
							str += '</td></tr></table>';
						}
						
						if ((progvb > 2) && (typeof prog.category != 'undefined')) {
							if (str1 != '') str1 += ' ';
							str1 += find('category', prog.category) + '.';
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
								else		    str2 += 'Episode ';
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

							str2 += 'Assigned episode ' + prog.assignedepisode;

							if (str2 != '') str2 += '.';

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

						if (str1 != '') {
							str += '<br><br>';
							str += str1;
						}

						str1 = '';

						if ((progvb > 3) &&
							!prog.flags.recorded  	   &&
							!prog.flags.scheduled 	   &&
							!prog.flags.rejected   	   &&
							!prog.flags.failed     	   &&
							!prog.flags.running        &&
							!prog.flags.postprocessing &&
							(typeof prog.recorded  == 'undefined') &&
							(typeof prog.scheduled == 'undefined') &&
							((typeof prog.dvbchannel != 'undefined') && (prog.dvbchannel != ''))) {
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
							if ((typeof prog.category != 'undefined') && (prog.category.toLowerCase() == 'film')) {
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

								if ((typeof prog.series == 'undefined') ||
									((typeof prog.series[j] != 'undefined') && (prog.series[j].state != 'empty'))) {
									if (series) {
										str1 += findfromfilter('Combined', 'title="' + prog.title + '" series=' + j, '', j ? 'Series ' + strpad(j, 2) : 'Unknown', 'View series ' + j + ' scheduled/recorded of this programme');
									}
									else {
										var en1 = j * 100 + 1, en2 = en1 + 99;
										str1 += findfromfilter('Combined', 'title="' + prog.title + '" episode>=' + en1 + ' episode<=' + en2, '', 'E' + strpad(en1, dec, '0') + ' to E' + strpad(en2, dec, '0'), 'View episode range scheduled/recorded of this programme');
									}

									if ((typeof prog.series != 'undefined') && (typeof prog.series[j] != 'undefined')) {
										str1 += ': <span class="episodelist">';
										for (k = 0; k < prog.series[j].episodes.length; k++) {
											var pattern, alttext;

											if (series) {
												pattern = 'series=' + j + ' episode=' + (k + 1);
												alttext = 'Series ' + j + ' Episode ' + (k + 1);
											}
											else {
												pattern = 'episode=' + (j * 100 + 1 + k);
												alttext = 'Episode ' + (j * 100 + 1 + k);
											}

											if		(prog.series[j].episodes[k] == 'r') alttext += ' (Recorded)';
											else if (prog.series[j].episodes[k] == 's') alttext += ' (Scheduled)';
											else if (prog.series[j].episodes[k] == 'x') alttext += ' (Rejected)';
											else if (prog.series[j].episodes[k] == '-') alttext += ' (Unknown)';

											str1 += findfromfilter('Combined', 'title="' + prog.title + '" ' + pattern, '', prog.series[j].episodes[k], alttext);
											
											if ((k % 10) == 9) str1 += ' ';
										}
										str1 += '</span><br>';
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
											   'uuid!="' + prog.uuid + '" ' + 'stop>"' + (new Date(prog.start)).toISOString() + '" ' + 'start<"' + (new Date(prog.stop)).toISOString() + '"',
											   '',
											   'Listings',
											   'Search for clashes in listings with this programme');
						str1 += '</td><td>';
						str1 += findfromfilter('Combined',
											   'uuid!="' + prog.uuid + '" ' + 'stop>"' + (new Date(prog.start)).toISOString() + '" ' + 'start<"' + (new Date(prog.stop)).toISOString() + '"',
											   '',
											   'Combined',
											   'Search for clashes in scheduled/recorded programmes with this programme');
						str1 += '</td><td>';
						str1 += findfromfilter('Requested',
											   'uuid!="' + prog.uuid + '" ' + 'stop>"' + (new Date(prog.start)).toISOString() + '" ' + 'start<"' + (new Date(prog.stop)).toISOString() + '"',
											   '',
											   'Requested',
											   'Search for clashes in requested programmes with this programme');
						str1 += '</td><td>';
						str1 += findfromfilter('Recorded',
											   'uuid!="' + prog.uuid + '" ' + 'stop>"' + (new Date(prog.start)).toISOString() + '" ' + 'start<"' + (new Date(prog.stop)).toISOString() + '"',
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
							str += '<br><br>';
							str += str1;
						}

						str += '</td></tr>';
					}

					response.progs[i].html = {};
					response.progs[i].html.selected = selected;
					response.progs[i].html.str      = str;
				}

				var reltimestr = '';
				if (typeof prog.start != 'undefined') {
					var dt = new Date();
					var offset = dt.getTime() - prog.start;
					var type = 'Start';
					var str, str1 = '', str2 = '';

					if (offset >= 0) {
						offset = dt.getTime() - prog.stop;
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

				astr += response.progs[i].html.str.replace('{reltime}', reltimestr);
			}
			
			astr += '</table>';
		}
	}
	else {
		document.getElementById("status").innerHTML = "No programmes";
	}

	return astr;
}

function recordprogramme(id)
{
	if (typeof response.progs[id] != 'undefined') {
		var user = document.getElementById('addrec' + id + 'user').value;
		var prog = response.progs[id];
		var pattern = 'title="' + prog.title + '"';
		var postdata = '';

		if (user == defaultuser) user = '';

		if (prog.category.toLowerCase() == 'film') pattern += ' category=' + prog.category.toLowerCase() + ' dir:="Films" onceonly:=1';
		
		if (typeof prog.subtitle != 'undefined') pattern += ' subtitle="' + prog.subtitle + '"';
		
		postdata += "editpattern=add\n";
		postdata += "newuser=" + user + "\n";
		postdata += "newpattern=" + pattern + "\n";
		postdata += "schedule=commit\n";

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
		postdata += "schedule=commit\n";

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
		else					     str += 'if&nbsp;';
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
			str += '><input type="text" id="pattern' + patternid + 'term' + i + 'valuevalue" value="' + term.value.replace(/"/g, '&quot;') + '" onchange="displayfilter_changed(' + patternid + ', ' + i + ', &quot;value&quot;)" />';
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
	else					  classname = 'disabled';
	str += '<tr class="' + classname + '"><td>';
	if ((typeof pattern.errors != 'undefined') && (pattern.errors != '')) {
		str += '<div title="Errors: ' + pattern.errors + '"><b>Error</b></div>';
	}
	else if (pattern.enabled) str += 'Enabled';
	else					  str += 'Disabled';

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
			else			str += defaultuser;
			str += '</option>';
		}
	}
	else {
		if (pattern.user != '') str += pattern.user;
		else				    str += defaultuser;
	}

	str += '</td><td class="desc">';
	//str += '<span id="pattern' + index + 'terms">' + displayfilter(index, -1, '') + '</span>';
	if (typeof pattern.addrec != 'undefined') {
		str += '<input type="text" id="newrecpattern" value="' + pattern.pattern.replace(/"/g, '&quot;') + '" onchange="addrecchanged(&quot;newrec&quot;)"/>';
	}
	else {
		str += '<input type="text" id="pattern' + index + 'pattern" value="' + pattern.pattern.replace(/"/g, '&quot;') + '" onchange="patternchanged(' + index + ')"/>';
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
	else			     str += '<button onclick="enablepattern(' + index + ')">Enable</button>&nbsp;';
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
	var i;

	if ((typeof response.patterns != 'undefined') && (response.for > 0)) {
		var status = showstatus('patterns');

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
					else			str += defaultuser;
					str += '</option>';
				}
			}
			else {
				if (pattern.user != '') str += pattern.user;
				else				    str += defaultuser;
			}
			str += '</td><td class="desc">';
			str += '<input type="text" id="newrecpattern" size=50 maxlength=10000 value="" onchange="addrecchanged(&quot;newrec&quot;)"/>';
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

	return str;
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
		else	     document.getElementById("updatepattern" + index + "link").innerHTML = "Update";
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
			
			dvbrequest(filterlist.current, postdata);
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
			
			dvbrequest(filterlist.current, postdata);
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
			
			dvbrequest(filterlist.current, postdata);
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
			
			dvbrequest(filterlist.current, postdata);
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

		dvbrequest(filterlist.current, postdata);
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

	return str;
}

function populate(id)
{
	var str = '';

	if (response != null) {
		if		(typeof response.patterns != 'undefined') str += populatepatterns(id);
		else if (typeof response.loglines != 'undefined') str += populatelogs(id);
		else if (typeof response.progs    != 'undefined') str += populateprogs(id);

		populateusers();
	}

	displayfilter(0, -1, '');

	document.getElementById("list").innerHTML = str;
}

function dvbrequest(filter, postdata)
{
	if (typeof filter != 'undefined') {
		if (typeof filter.from 	      == 'undefined') filter.from 		 = filterlist.current.from;
		else										  document.getElementById("from").value = filter.from;
		if (typeof filter.titlefilter == 'undefined') filter.titlefilter = filterlist.current.titlefilter;
		else										  document.getElementById("titlefilter").value = filter.titlefilter;
		if (typeof filter.timefilter  == 'undefined') filter.timefilter  = filterlist.current.timefilter;
		else										  document.getElementById("timefilter").value = filter.timefilter;
		if (typeof filter.page 	      == 'undefined') {
			if ((filter.from        != filterlist.current.from) ||
				(filter.titlefilter != filterlist.current.titlefilter) ||
				(filter.timefilter  != filterlist.current.timefilter)) {
				filter.page = 0;
			}
			else filter.page = filterlist.current.page;
		}
		if (typeof filter.pagesize    == 'undefined') filter.pagesize    = filterlist.current.pagesize;
		else										  document.getElementById("pagesize").value = filter.pagesize;
 		if (typeof filter.expanded    == 'undefined') {
			if ((filter.from        != filterlist.current.from) ||
				(filter.titlefilter != filterlist.current.titlefilter) ||
				(filter.timefilter  != filterlist.current.timefilter) ||
				(filter.page        != filterlist.current.page)) {
				filter.expanded = -1;
			}
			else filter.expanded = filterlist.current.expanded;
		}
	}
	else filter = filterlist.current;

	filter.page     = filter.page     | 0;
	filter.pagesize = filter.pagesize | 0;

	if (((filterlist.current == null) ||
		 (typeof postdata    != 'undefined') ||
		 ((typeof filter.fetch != 'undefined') && filter.fetch) ||
		 (filter.from        != filterlist.current.from) ||
		 (filter.titlefilter != filterlist.current.titlefilter) ||
		 (filter.timefilter  != filterlist.current.timefilter) ||
		 (filter.page        != filterlist.current.page) ||
		 (filter.pagesize    != filterlist.current.pagesize)) &&
		!((typeof filter.fetch != 'undefined') && !filter.fetch)) {
		document.getElementById("status").innerHTML = '<span style="font-size:200%;">Fetching...</span>';
		
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

						populate(filter.expanded);

						updatefilterlist(filter);
					}
					else document.getElementById("status").innerHTML = "<h1>Server returned error " + xmlhttp.status + "</h1>";
				}
			}
		}
		
		xmlhttp.open("POST", "dvb.php", true);

		var data = "";
		data += "from=" + filter.from + "\n";
		data += "filter=" + getfullfilter(filter) + "\n";
		data += "titlefilter=" + filter.titlefilter + "\n";
		data += "timefilter=" + filter.timefilter + "\n";
		data += "page=" + filter.page + "\n";
		data += "pagesize=" + filter.pagesize + "\n";
		if (searches != null) {
			data += "searchesref=" + searches.ref + "\n";
		}
		if (patterndefs == null) {
			data += "patterndefs=\n";
		}
		if (typeof postdata != 'undefined') data += postdata;
		xmlhttp.send(data);

		document.getElementById("status").innerHTML += " <button onclick=\"abortfind()\">Abort</button>";
		document.getElementById("statusbottom").innerHTML = '';
	}
	else {
		populate(filter.expanded);

		updatefilterlist(filter);
	}
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

function comparefilter(filter, index)
{
	var filter2 = filterlist.list[index];

	return ((filter2.from     	 == filter.from)   	    &&
			(filter2.titlefilter == filter.titlefilter) &&
			(filter2.timefilter  == filter.timefilter)  &&
			(filter2.page     	 == filter.page)		&&
			(filter2.pagesize 	 == filter.pagesize)    &&
			(filter2.expanded    == filter.expanded));
}

function generatefilterdescription(filter)
{
	var str, fullfilter = getfullfilter(filter);
	var title;

	str = 'Page ' + (filter.page + 1) + ' of ' + filter.from;
	if (fullfilter != '') {
		str += '\nFiltered using \'' + limitstring(fullfilter) + '\'';
	}
	if ((filter.expanded >= 0) && (typeof response.progs != 'undefined') && (filter.expanded < response.progs.length)) {
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
	if ((filter.expanded >= 0) && (typeof response.progs != 'undefined')) {
		str += ' (' + title + ')';
	}
	filter.shortdesc = str;

	return str;
}

function updatefilterlist(filter)
{
	var i, str = '';

	generatefilterdescription(filter);
	filterlist.current = filter;

	if (filterlist.index >= filterlist.list.length) {
		filterlist.list[filterlist.index] = filter;
	}
	else if (!comparefilter(filterlist.current, filterlist.index)) {
		filterlist.index++;
		filterlist.list[filterlist.index] = filter;
	}
	
	while (filterlist.list.length >= 100) {
		filterlist.list.shift();
		if (filterlist.index >= 0) filterlist.index--;
	}

	str += '<button ';
	if (filterlist.index > 0) {
		str += 'onclick="filterlist.index = ' + (filterlist.index - 1) + '; dvbrequest(filterlist.list[filterlist.index])"';
	}
	else str += 'disabled';
	str += '>Back</button>&nbsp;&nbsp;<button ';

	if ((filterlist.index + 1) < filterlist.list.length) {
		str += 'onclick="filterlist.index = ' + (filterlist.index + 1) + '; dvbrequest(filterlist.list[filterlist.index])"';
	}
	else str += 'disabled';
	str += '>Forward</button>&nbsp;&nbsp;';

	str += '<select id="filterlist_select" onchange="selectfilter()">';
	for (i = 0; i < filterlist.list.length; i++) {
		var filter = filterlist.list[i];
		str += '<option';
		if (filterlist.index == (i)) str += ' selected';
		str += ' value="' + i + '" title="' + filter.longdesc.replace(/"/g, '&quot;') + '">' + filter.shortdesc + '</option>';
	}
	str += '</select>';

	if (filterlist.current != null) {
		link = '?from=' + filterlist.current.from + '&titlefilter=' + encodeURIComponent(filterlist.current.titlefilter) + '&timefilter=' + encodeURIComponent(filterlist.current.timefilter) + '&page=' + filterlist.current.page + '&pagesize=' + filterlist.current.pagesize;
		str += '&nbsp;&nbsp;<a href="' + link.replace(/&/g, '&amp;') + '" target=_blank>Link</a>';
	}
	else link = null;
	
	document.getElementById("filterlist").innerHTML = str;
}

function selectfilter()
{
	filterlist.index = document.getElementById("filterlist_select").value | 0;
	dvbrequest(filterlist.list[filterlist.index]);
}

function decodeurl()
{
	var filter = {};
	var search = location.search.substring(1);

	if ((typeof search != 'undefined') && (search != '')) {
		filter = JSON.parse('{"' + search.replace(/&/g, '","').replace(/=/g,'":"') + '"}',
							function(key, value) {return key === "" ? value : decodeURIComponent(value);});
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
