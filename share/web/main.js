var defaultuser = 'default';
var defaulttimefilter = 'stop>=yesterday,0:0:0';

var response = null;
var patterns = [];
var editingpattern = -1;
var searchpattern = null;
var patterndefs = null;
var xmlhttp  = null;
var filterlist = {
	index:-1,
	current:null,
	list:[]
};
var filters = [
	{text:"Sources:<br>"},
	{
		title:"Listings",
		filter:{from:"Listings",titlefilter:"",timefilter:"stop>now"},
	},
	{
		title:"Combined",
		filter:{from:"Combined",titlefilter:"",timefilter:defaulttimefilter},
	},
	{
		title:"Requested",
		filter:{from:"Requested",titlefilter:"",timefilter:""},
	},
	{
		title:"Rejected",
		filter:{from:"Combined",titlefilter:"rejected=1",timefilter:defaulttimefilter},
	},
	{
		title:"Scheduled",
		filter:{from:"Combined",titlefilter:"scheduled=1",timefilter:defaulttimefilter},
	},
	{
		title:"Recorded",
		filter:{from:"Combined",titlefilter:"recorded=1",timefilter:defaulttimefilter},
	},
	{
		title:"Patterns",
		filter:{from:"Patterns"},
	},
	{
		title:"Clear Filter",
		filter:{titlefilter:""},
	},
	{
		title:"Sky",
		filter:{from:"Sky",titlefilter:"",timefilter:"stop>now"},
	},
	{text:"<br>DVB Logs:<br>"},
	{
		title:"Today",
		filter:{from:"Logs",timefilter:"start=today"},
	},
	{
		title:"Yesterday",
		filter:{from:"Logs",timefilter:"start=yesterday"},
	},
];
var searches = null;

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
	
	dvbrequest(decodeurl());
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

	return '<a href="javascript:void(0);" onclick="dvbrequest({&quot;titlefilter&quot;:&quot;' + titlefilter + '&quot;});" ' + style + ' title="' + title + '">' + str + '</a>';
}

function findfromfilter(from, titlefilter, timefilter, str, title, style)
{
	if (typeof title 	  == 'undefined') title = 'Search ' + from + ' using filter ' + filter.replace(/"/g, '&quot;');
	if (typeof style 	  == 'undefined') style = '';
	if (typeof timefilter == 'undefined') timefilter = '';

	titlefilter = titlefilter.replace(/"/g, '\\&quot;');
	timefilter  = timefilter.replace(/"/g, '\\&quot;');

	return ('<a href="javascript:void(0);" onclick="dvbrequest({' +
			'&quot;from&quot;:&quot;'        + from        + '&quot;,' +
			'&quot;titlefilter&quot;:&quot;' + titlefilter + '&quot;,' + 
			'&quot;timefilter&quot;:&quot;'  + timefilter  + '&quot;});" ' +
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
				status += ' <a href="javascript:void(0);" onclick="dvbrequest()">First</a>';

				status += ' <a href="javascript:void(0);" onclick="dvbrequest({&quot;page&quot;:' + (page - 1) + '})">Previous</a>';
			}

			for (i = 0; i < maxpage; i++) {
				if (!i || (i == (maxpage - 1)) ||
					((i < (page + 2)) && ((i + 2) > page))) {
					status += ' <a href="javascript:void(0);" onclick="dvbrequest({&quot;page&quot;:' + i + '})">';
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
				status += ' <a href="javascript:void(0);" onclick="dvbrequest({&quot;page&quot;:' + (page + 1) + '})">Next</a>';
				
				status += ' <a href="javascript:void(0);" onclick="dvbrequest({&quot;page&quot;:' + (maxpage - 1) + '})">Last</a>';
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

	dvbrequest({"page":page});
}

function calctime(length)
{
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

	if (typeof prog.actstarttime != 'undefined') {
		if (str != '') str += '<br><br>';
		str += 'Recorded as \'' + prog.filename + '\' (' + prog.actstartdate + ' ' + prog.actstarttime + ' - ' + prog.actstoptime + ': ' + calctime(prog.actlength) + ')';
		if (typeof prog.filesize != 'undefined') str += ' filesize ' + ((prog.filesize / (1024 * 1024)) | 0) + 'MB';

		str += addcarddetails(prog);
		str += '.';

		if (typeof prog.exists != 'undefined') {
			str += ' Programme <b>' + (prog.exists ? 'Available' : 'Unavailable') + '</b>.';
		}
	}
	else if (typeof prog.recstarttime != 'undefined') {
		if (str != '') str += '<br><br>';
		str += 'To be recorded as \'' + prog.filename + '\' (' + prog.recstartdate + ' ' + prog.recstarttime + ' - ' + prog.recstoptime + ': ' + calctime(prog.reclength) + ')';

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
		str += '<tr><th>User</th><th>Default Folder</th><th>Freespace</th><th>Lists</th></tr>';

		for (i = 0; i < response.users.length; i++) {
			var user     = response.users[i];
			var username = user.user;
			var level    = user.level + 0;

			if (level > 2) level = 2;

			str += '<tr class="diskspacelevel' + level + '"><td>';
			if (username != '') str += username;
			else				str += defaultuser;
			str += '</td><td style="text-align:left;">' + user.fullfolder + '</td>';

			if (typeof user.freespace != 'undefined') {
				str += '<td>' + user.freespace + '</td>';
			}
			else str += '<td>Unknown</td>';

			str += '<td>';
			if ((username == 'default') || (username == '')) {
				str += findfromfilter('Combined', '', defaulttimefilter, 'Combined', 'Search for recordings in combined listings for the default user') + '&nbsp;';
				str += findfromfilter('Requested', '', '', 'Requested', 'Search for requested recordings for the default user') + '&nbsp;';
				str += findfromfilter('Patterns', 'user=""', '', 'Patterns', 'Display scheduling patterns for the default user') + '&nbsp;';
			}
			else {
				str += findfromfilter('Combined', 'user="' + username + '"', defaulttimefilter, 'Combined', 'Search for recordings in combined listings for user ' + username) + '&nbsp;';
				str += findfromfilter('Requested', 'user="' + username + '"', '', 'Requested', 'Search for requested recordings for user ' + username) + '&nbsp;';
				str += findfromfilter('Patterns', 'user="' + username + '"', '', 'Patterns', 'Display scheduling patterns for user ' + username) + '&nbsp;';
			}
			str += '</td>';
			str += '</tr>';
		}

		if (typeof response.diskspace != 'undefined') {
			str += '<tr><th>User</th><th>Folders Used By Patterns</th><th>Freespace</th><th>State</th></tr>';

			for (i = 0; i < response.diskspace.length; i++) {
				var diskspace  = response.diskspace[i];
				var username   = diskspace.user;
				var level      = diskspace.level + 0;

				if (level > 2) level = 2;

				str += '<tr class="diskspacelevel' + level + '"><td>';
				if (username != '') str += username;
				else				str += defaultuser;
				str += '</td><td style="text-align:left;">' + diskspace.fullfolder + '</td>';

				if (typeof diskspace.freespace != 'undefined') {
					str += '<td>' + diskspace.freespace;
				}
				else str += '<td>Unknown';

				str += '</td>';

                str += '<td>';
				str += findfromfilter('Combined', 'filename@<"' + diskspace.fullfolder + '"', defaulttimefilter, 'Combined', 'Search for recordings in combined listings with folder ' + diskspace.folder) + '&nbsp;';
				str += findfromfilter('Requested', 'filename@<"' + diskspace.fullfolder + '"', '', 'Requested', 'Search for requested recordings with folder ' + diskspace.folder) + '&nbsp;';
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
	var numstr = num + '';
	return ('0000' + numstr).slice(-Math.max(len, numstr.length));
}

function populateprogs(id)
{
	var str = '';
	var i;

	displayfilter(0, -1, '');

	if ((typeof response.progs != 'undefined') && (response.for > 0)) {
		var verbosity = document.getElementById("verbosity").value;
		var status = showstatus('programmes');

		document.getElementById("status").innerHTML = status;
		document.getElementById("statusbottom").innerHTML = status;

		if (response.progs.length > 0) {
			str += '<table class="proglist">';

			patterns = [];
			patterns[0] = searchpattern;

			for (i = 0; i < response.progs.length; i++) {
				var prog      = response.progs[i];
				var selected  = (i == id);
				var classname = '';

				if		(prog.flags.running)												 classname = ' class="recording"';
				else if	(prog.flags.recorded  || (typeof prog.recorded  != 'undefined'))   	 classname = ' class="recorded"';
				else if (prog.flags.scheduled || (typeof prog.scheduled != 'undefined')) 	 classname = ' class="scheduled"';
				else if (prog.flags.rejected  || (typeof prog.rejected  != 'undefined'))	 classname = ' class="rejected"';
				else if ((typeof prog.category != 'undefined') && (prog.category == 'Film')) classname = ' class="film"';

				str += '<tr' + classname + ' onclick="populate(' + (selected ? -1 : i) + ')">'
				str += '<td>';
				str += find('start', prog.startdate, 'Search for programmes on this day');
				str += '</td><td>';
				str += findfilter('stop>"' + prog.starttime + '" start<"' + prog.stoptime + '"', prog.starttime + ' - ' + prog.stoptime, 'Search for programmes during these times');
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
				else if (typeof prog.assignedepisode != 'undefined') {
					str += '<span style="font-size:90%;">F' + prog.assignedepisode + '</span>';
				}
				else str += '&nbsp;';

				str += '</td><td>';
				if (typeof prog.user != 'undefined') str += find('user', prog.user, 'Seach for programmes assigned to this user');
				else							     str += '&nbsp;';

				str += '</td><td class="title">';
				str += find('title', prog.title) + imdb(prog.title);

				if (typeof prog.subtitle != 'undefined') {
					str += ' / ' + find('subtitle', prog.subtitle) + imdb(prog.subtitle);
				}

				if (typeof prog.startoffset != 'undefined') {
					var dt = new Date();
					var offset = dt.getTime() - prog.startoffset;
					var type = 'Start';
					var str1 = '', str2 = '';

					if (offset >= 0) {
						offset = dt.getTime() - prog.stopoffset;
						type = 'End';
					}

					str += ' <span style="font-size:80%;">(';
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
				}
				str += '</td></tr>';

				var progvb = selected ? 10 : verbosity;
				if (progvb > 1) {
					var str1 = '';

					//str += '<tr' + classname + ' onclick="populate()"><td class="desc" colspan=5>';
					str += '<tr' + classname + '><td class="desc" colspan=6>';

					if ((typeof prog.rejected != 'undefined') && prog.rejected) {
						str += '<span style="font-size:150%;">-- Rejected --</span><br><br>';
					}

					if ((progvb > 1) && (typeof prog.desc != 'undefined')) {
						str += prog.desc;
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
						(typeof prog.recorded  == 'undefined') &&
						(typeof prog.scheduled == 'undefined') &&
						(typeof prog.rejected  == 'undefined')) {
						//str1 += '<br><br>';
						if (typeof response.users != 'undefined') {
							str1 += '<select class="addrecord" id="addrec' + i + 'user">';
							for (j = 0; j < response.users.length; j++) {
								var user = response.users[j].user;

								str1 += '<option>';
								if (user != '') str1 += user;
								else			str1 += defaultuser;
								str1 += '</option>';
							}
							str1 += '</select>';
						}
						else str1 += defaultuser;

						str1 += '&nbsp;&nbsp;&nbsp;';
						str1 += '<button class="addrecord" onclick="recordprogramme(' + i + ')">Record Programme</button>';
						if (prog.category != 'Film') {
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
						for (j = 1; j < n; j++) {
							if (typeof prog.series == 'undefined') str1 += '&nbsp;&nbsp;&nbsp;';

							if ((typeof prog.series == 'undefined') ||
								((typeof prog.series[j] != 'undefined') && (prog.series[j].state != 'empty'))) {
								if (series) str1 += findfromfilter('Combined', 'title="' + prog.title + '" series=' + j, '', 'Series ' + strpad(j, 2), 'View series ' + j + ' scheduled/recorded of this programme');
								else {
									var en1 = j * 100 - 99, en2 = en1 + 99;
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
											pattern = 'episode=' + (j * 100 - 99 + k);
											alttext = 'Episode ' + (j * 100 - 99 + k);
										}

										if		(prog.series[j].episodes[k] == 'r') alttext += ' (Recorded)';
										else if (prog.series[j].episodes[k] == 's') alttext += ' (Scheduled)';
										else if (prog.series[j].episodes[k] == 'x') alttext += ' (Rejected)';
										else if (prog.series[j].episodes[k] == '-') alttext += ' (Unknown)';

										str1 += findfromfilter('Combined', 'title="' + prog.title + '" ' + pattern, '', prog.series[j].episodes[k], alttext);
									}
									str1 += '</span><br>';
								}
							}
						}

						if (typeof prog.series == 'undefined') str1 += '<br>';

						str1 += '<br>';
					}

					if ((progvb > 3) && (typeof prog.pattern != 'undefined')) {
						if (str1 != '') str1 += ' ';

						str1 += 'Found using filter \'' + limitstring(prog.pattern) + '\'';
						if (typeof prog.user != 'undefined') {
							str1 += ' by user \'' + prog.user + '\'';
						}
						if (typeof prog.pri != 'undefined') {
							str1 += ', priority ' + prog.pri;
						}
						if ((typeof prog.score != 'undefined') && (prog.score > 0)) {
							str1 += ' (score ' + prog.score + ')';
						}
						str1 += ':';

						if (((typeof prog.recorded  != 'undefined') ||
							 (typeof prog.scheduled != 'undefined')) &&
							(typeof prog.patternparsed != 'undefined') &&
							(prog.patternparsed != '')) {
							str1 += '<table class="patternlist">';
							str1 += '<tr><th>Status</th><th>Priority</th><th>User</th><th class="desc">Pattern</th></tr>';
							str1 += populatepattern(prog.patternparsed);
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
					else str2 = addtimesdata(prog);

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
					str1 += '</td></tr>';
					str1 += '<tr><td>Title</td><td>';
					str1 += findfromfilter('Listings', 'title="' + prog.title + '"', '', 'Listings', 'Search for occurrences of programmes with this title');
					str1 += '</td><td>';
					str1 += findfromfilter('Combined', 'title="' + prog.title + '"', '', 'Combined', 'Search for scheduled/recorded occurrences of programmes with this title');
					str1 += '</td><td>';
					str1 += findfromfilter('Requested', 'title="' + prog.title + '"', '', 'Requested', 'Search for requested recordings of programmes with this title');
					str1 += '</td></tr>';
					if (typeof prog.subtitle != 'undefined') {
						str1 += '<tr><td>Title / Subtitle</td><td>';
						str1 += findfromfilter('Listings', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Listings', 'Search for occurrences of programmes with this title and subtitle');
						str1 += '</td><td>';
						str1 += findfromfilter('Combined', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Combined', 'Search for scheduled/recorded occurrences of programmes with this title and subtitle');
						str1 += '</td><td>';
						str1 += findfromfilter('Requested', 'title="' + prog.title + '" subtitle="' + prog.subtitle + '"', '', 'Requested', 'Search for requested recordings of programmes with this title and subtitle');
						str1 += '</td></tr>';
					}
					str1 += '<tr><td>Clashes</td><td>';
					str1 += findfromfilter('Listings',
										   'uuid!=="' + prog.uuid + '"',
										   'stop>"' + prog.start + '" ' +
										   'start<"' + prog.stop + '" ',
										   'Listings',
										   'Search for clashes in listings with this programme');
					str1 += '</td><td>';
					str1 += findfromfilter('Combined',
										   'uuid!=="' + prog.uuid + '"',
										   'stop>"' + prog.start + '" ' +
										   'start<"' + prog.stop + '"',
										   'Combined',
										   'Search for clashes in scheduled/recorded programmes with this programme');
					str1 += '</td><td>';
					str1 += findfromfilter('Requested',
										   'uuid!=="' + prog.uuid + '"',
										   'stop>"' + prog.start + '" ' +
										   'start<"' + prog.stop + '"',
										   'Requested',
										   'Search for clashes in requested programmes with this programme');
					str1 += '</td></tr></table>';

					if (str1 != '') {
						str += '<br><br>';
						str += str1;
					}

					str += '</td></tr>';
				}
			}
			str += '</table>';
		}
	}
	else {
		document.getElementById("status").innerHTML = "No programmes";
	}

	return str;
}

function recordprogramme(id)
{
	if (typeof response.progs[id] != 'undefined') {
		var user = document.getElementById('addrec' + id + 'user').value;
		var prog = response.progs[id];
		var pattern = 'title="' + prog.title + '"';
		var postdata = '';

		if (typeof prog.subtitle != 'undefined') pattern += ' subtitle="' + prog.subtitle + '"';
		
		postdata += "edit=add\n";
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
		
		postdata += "edit=add\n";
		postdata += "newuser=" + user + "\n";
		postdata += "newpattern=" + pattern + "\n";
		postdata += "schedule=commit\n";

		dvbrequest({from:"Combined", titlefilter:pattern, timefilter:defaulttimefilter}, postdata);
	}
}

function addrecfromlisting(id)
{
	if (typeof response.progs[id] != 'undefined') {
		var user = document.getElementById('addrec' + id + 'user').value;
		var prog = response.progs[id];
		var pattern = 'title="' + prog.title + '"';
		var postdata = '';
		
		postdata += "edit=add\n";
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

function populatepattern(pattern)
{
	var index = patterns.length;
	var str = '', classname = '';
	var j, k;

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
	str += (pattern.pri | 0);
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

function patternchanged(index)
{
	if (typeof patterns[index] != 'undefined') {
		var updated = (( document.getElementById("pattern" + index + "pattern").value != patterns[index].pattern) ||
					   ( document.getElementById("pattern" + index + "user").value    != patterns[index].user));
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
				postdata += "edit=update\n";
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

			postdata += "edit=enable\n";
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

			postdata += "edit=disable\n";
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

			postdata += "edit=delete\n";
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

		postdata += "edit=add\n";
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
		if (typeof response.patterns != 'undefined') str += populatepatterns(id);
		if (typeof response.loglines != 'undefined') str += populatelogs(id);
		if (typeof response.progs    != 'undefined') str += populateprogs(id);

		populateusers();
	}

	displayfilter(0, -1, '');

	document.getElementById("list").innerHTML = str;
}

function dvbrequest(filter_arg, postdata)
{
	document.getElementById("status").innerHTML = '<span style="font-size:200%;">Fetching...</span>';

	var filter = {
		from:document.getElementById("from").value,
		titlefilter:document.getElementById("titlefilter").value,
		timefilter:document.getElementById("timefilter").value,
		page:0,
		pagesize:document.getElementById("pagesize").value
	};

	if (typeof filter_arg != 'undefined') {
		if (typeof 	filter_arg.from 	   != 'undefined') filter.from 		  = filter_arg.from;
		if (typeof 	filter_arg.titlefilter != 'undefined') filter.titlefilter = filter_arg.titlefilter;
		if (typeof 	filter_arg.timefilter  != 'undefined') filter.timefilter  = filter_arg.timefilter;
		if ((typeof filter_arg.page 	   != 'undefined') && (filter_arg.page >= 0)) filter.page = filter_arg.page;
		if (typeof  filter_arg.pagesize    != 'undefined') filter.pagesize    = filter_arg.pagesize;
	}

	document.getElementById("from").value        = filter.from;
	document.getElementById("titlefilter").value = filter.titlefilter;
	document.getElementById("timefilter").value  = filter.timefilter;
	document.getElementById("pagesize").value    = filter.pagesize;

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

					populate();
				}
				else document.getElementById("status").innerHTML = "<h1>Server returned error " + xmlhttp.status + "</h1>";
			}
		}
		
		while (filterlist.list.length >= 100) {
			filterlist.list.shift();
			if (filterlist.index >= 0) filterlist.index--;
		}
		
		filterlist.current = JSON.parse(JSON.stringify(filter));

		var newindex;
		if ((newindex = findfilterinlist(filter)) >= 0) {
			filterlist.index = newindex;
		}
		else if (filterlist.index < (filterlist.list.length - 1)) {
			filterlist.list[filterlist.index] = filter;
		}
		else {
			filterlist.list.push(filter);
			filterlist.index = filterlist.list.length - 1;
		}
		
		updatefilterlist();

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
			(filter2.pagesize 	 == filter.pagesize));
}

function findfilterinlist(filter)
{
	var i;

	for (i = 0; i < filterlist.list.length; i++) {
		if (comparefilter(filter, i)) {
			return i;
		}
	}

	return -1;
}

function updatefilterlist()
{
	var i, str = '';

	str += '<button ';
	if (filterlist.index > 0) {
		str += 'onclick="dvbrequest(filterlist.list[' + (filterlist.index - 1) + '])"';
	}
	else str += 'disabled';
	str += '>Back</button>&nbsp;&nbsp;<button ';

	if ((filterlist.index + 1) < filterlist.list.length) {
		str += 'onclick="dvbrequest(filterlist.list[' + (filterlist.index + 1) + '])"';
	}
	else str += 'disabled';
	str += '>Forward</button>&nbsp;&nbsp;';

	str += '<select id="filterlist_select" onchange="selectfilter()">';
	for (i = 0; i < filterlist.list.length; i++) {
		var filter = filterlist.list[filterlist.list.length - 1 - i];
		str += '<option';
		if (filterlist.index == (filterlist.list.length - 1 - i)) str += ' selected';
		str += ' value="' + (filterlist.list.length - 1 - i) + '">Page ' + ((filter.page | 0) + 1) + ' of ' + filter.from + ' using filter \'' + limitstring(getfullfilter(filter)) + '\' (pagesize ' + filter.pagesize + ')</option>';
	}
	str += '</select>';

	if (filterlist.current != null) {
		str += '&nbsp;&nbsp;<a href="?from=' + filterlist.current.from + '&amp;titlefilter=' + encodeURIComponent(filterlist.current.titlefilter) + '&amp;timefilter=' + encodeURIComponent(filterlist.current.timefilter) + '&amp;page=' + filterlist.current.page + '&amp;pagesize=' + filterlist.current.pagesize + '" target=_blank>Link</a>';
	}
	
	document.getElementById("filterlist").innerHTML = str;
}

function selectfilter()
{
	var index = document.getElementById("filterlist_select").value | 0;
	dvbrequest(filterlist.list[index]);
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
		dvbrequest({"page":(response.from / (document.getElementById("pagesize").value | 0))});
	}
	else dvbrequest();
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
		dvbrequest(filterlist.current, 'schedule=commit');
	}
}
