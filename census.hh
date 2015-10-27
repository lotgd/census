<?hh

require './vendor/autoload.php';

// Only report errors.
error_reporting(E_ERROR | E_PARSE | E_RECOVERABLE_ERROR);

LotGD\Census\Census::main($argv, $argc);
