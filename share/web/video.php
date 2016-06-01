<!DOCTYPE html>
<html>
<?php

 //print_r($_SERVER);

if (isset($_GET['prog'])) $prog = $_GET['prog'];
if (isset($argc) && ($argc > 1)) {
	$prog = $argv[1];
	$verbose = false;
}
else $verbose = false;

$script = $_SERVER['SCRIPTNAME'];

exec('dvbdecodeprog "' . $prog . '"', $res);
$prog = json_decode(implode('', $res), true);

if ($verbose) print_r($prog);

echo "<head>\n";
echo "<meta charset=\"UTF-8\">\n";
if (isset($prog['error'])) {
	echo "<h1>" . $prog['error'] . "</h1>";	
}
else {
	echo "<title>" . $prog['title'];
	if (isset($prog['subtitle'])) echo " / " . $prog['subtitle'];
	echo "</title>\n";
	echo "</head>\n";
	echo "<body>\n";

	echo "<h1>" . $prog['title'];
	if (isset($prog['subtitle'])) echo " / " . $prog['subtitle'];
	echo "</h1><br>\n";

	//print_r($prog);
	
	echo '<center><video autoplay controls>' . "\n";
	echo '<source src="' . $prog['file'] . '" />' . "\n";
	echo '</video></center>' . "\n";
	echo "<br><br>\n";

	if (isset($prog['start']) && isset($prog['stop'])) {
		echo "<h2>" . date('D d-M-Y H:i', $prog['start'] / 1000) . " - " . date('D d-M-Y H:i', $prog['stop'] / 1000);
		if (isset($prog['channel'])) echo " on " . $prog['channel'];
		echo "</h2>\n";
	}

	if (isset($prog['desc'])) echo $prog['desc'] . "<br><br>\n";

	if (isset($prog['category'])) echo "<b>" . $prog['category'] . "</b>.<br><br>\n";
	if (isset($prog['director'])) echo "Directed by <b>" . $prog['director'] . "</b>.<br><br>\n";
	if (isset($prog['episode'])) {
		if (isset($prog['episode']['series'])) {
			echo "Series " . $prog['episode']['series'];
			if (isset($prog['episode']['episode'])) {
				echo ", episode <b>" . $prog['episode']['episode'] . "</b>";
				if (isset($prog['episode']['episodes'])) echo " of <b>" . $prog['episode']['episodes'] . "</b>";
			}
		}
		else if (isset($prog['episode']['episode'])) {
			echo "Episode <b>" . $prog['episode']['episode'] . "</b>";
			if (isset($prog['episode']['episodes'])) echo " of <b>" . $prog['episode']['episodes'] . "</b>";
		}
		echo ".<br>\n";
		if (isset($prog['next'])) {
			echo "<h2>Next: <a href=\"$script?prog=" . urlencode($prog['next']['prog']) . "\">" . $prog['next']['desc'] . "</a></h2>\n";
		}
		if (isset($prog['previous'])) {
			echo "<h2>Previous: <a href=\"$script?prog=" . urlencode($prog['previous']['prog']) . "\">" . $prog['previous']['desc'] . "</a></h2>\n";
		}
	}
	if (isset($prog['actors'])) {
		echo "Starring:<ul>";
		for ($i = 0; $i < count($prog['actors']); $i++) {
			echo "<li>" . $prog['actors'][$i] . "</li>\n";
		}
		echo "</ul>\n";
	}
}
?>
</body>
<link rel="stylesheet" type="text/css" href="main.css" />
</html>
