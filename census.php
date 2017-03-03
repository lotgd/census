<?php

require './vendor/autoload.php';

// Only report errors.
error_reporting(E_ERROR | E_PARSE | E_RECOVERABLE_ERROR);

// Only run this every 3 days.
if (date('z') % 2 === 0) {
    LotGD\Census\Census::main($argv, $argc);
}
