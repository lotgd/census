<?hh

namespace LotGD\Census;

class Configuration {
  public bool $debugFakeData = false;
  public bool $debugNoWrites = false;
  public string $listURL = '/list.php?page=';
  public Repository $repo;

  public Logger $logger;

  public function __construct() {
    $logger = new Monolog\Logger('census');
    $this->logger = $logger;
    $this->logger->pushHandler(new Monolog\Handler\StreamHandler('php://stderr', Logger::WARNING));

    $this->repo = new Repository($logger);
  }

  public function readCommandLineArguments(array $argv, int $argc) {
    foreach ($argv as $a) {
      if ($a == '--fake-data') {
        $this->debugFakeData = true;
      }
      if ($a == '--no-writes') {
        $this->debugNoWrites = true;
      }
    }
  }
}
