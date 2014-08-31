<?php $args = file_get_contents("php://input"); passthru("dvbweb " . base64_encode($args));?>
