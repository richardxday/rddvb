<?php $args = file_get_contents("php://input"); passthru("/usr/local/bin/dvbweb " . base64_encode($args));?>
