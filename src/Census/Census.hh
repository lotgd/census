<?hh

namespace LotGD\Census;

use LotGD\Census\WarriorsPageState;

class Census {
  public bool $debugFakeData = false;
  public bool $debugNoWrites = false;
  public Repository $repo;

  public Logger $logger;

  public function __construct(\Monolog\Logger $logger) {
    $this->logger = $logger;
    $this->repo = new Repository($this->logger);
  }

  public function readCommandLineArguments(array $argv, int $argc) {
    foreach ($argv as $a) {
      if ($a == '--fake-data') {
        $this->debugFakeData = true;
      }
      if ($a == '--no-writes') {
        $this->debugNoWrites = true;
      }
      if ($a == '--debug') {
        $this->logger->setHandlers(array(new \Monolog\Handler\StreamHandler('php://stderr', \Monolog\Logger::DEBUG)));
      }
    }
  }

  public static function main(array $argv, int $argc) {
    $listURL = '/list.php?op=bypage&page=';

    $l = new \Monolog\Logger('census');
    $l->pushHandler(new \Monolog\Handler\StreamHandler('php://stderr', \Monolog\Logger::WARNING));

    $c = new Census($l);
    $c->readCommandLineArguments($argv, $argc);

    $sites = $c->repo->getSites();

    $statsMap = Map {};

    if ($c->debugFakeData) {
      $sites = Vector {};

      $stats = new Stats();
      $stats->count = 0;
      $stats->dau = 0;
      $stats->mau = 0;
      $statsMap['twx.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 103;
      $stats->dau = 12;
      $stats->mau = 23;
      $statsMap['site3'] = $stats;

      $c->logger->addDebug("Using fake data:");
      $c->logger->addDebug(var_export($statsMap, true));
    } else {
      foreach ($sites as $site) {
        $stats = new Stats();

        $i = 1;
        while ($contents = file_get_contents($u = 'http://' . $site . $listURL . $i)) {
          if ($i > 100) {
            $c->logger->addError("  too many pages");
            break;
          }

          $c->logger->addDebug("Fetching {$u}");
          $wp = new WarriorsPage($c->logger, $u, $contents);

          if ($wp->stats != null) {
            $stats->add($wp->stats);
          }
          if ($wp->state == WarriorsPageState::End || $wp->state == WarriorsPageState::Error) {
            break;
          }

          $i++;
        }

        $statsMap[$site] = $stats;
      }
    }

    $c->updateReadme($statsMap);

    $c->updateCSV('data/total.csv', $statsMap->map(function (Stats $s) : int {
      return $s->count;
    }));

    $c->updateCSV('data/mau.csv', $statsMap->map(function (Stats $s) : int {
      return $s->mau;
    }));

    $c->updateCSV('data/dau.csv', $statsMap->map(function (Stats $s) : int {
      return $s->dau;
    }));
  }

  private function updateCSV(string $file, Map<string, int> $map) {
    $contents = $this->repo->getFile($file);
    if ($contents === null) {
      $this->logger->addError("Couldn't get {file} from the repo.");
      return;
    }

    $csv = new CSV($this->logger, $file, $contents);
    $csv->appendToRows($map);
    $result = $csv->contents();

    if ($this->debugNoWrites) {
      $this->logger->addDebug("Would be writing {$file}:");
      $this->logger->addDebug($result);
    } else {
      $this->logger->addDebug("Writing {$file}:");
      $this->logger->addDebug($result);
      $this->repo->updateFile($file, $contents, 'The latest data.');
    }
  }

  private function updateReadme(Map<string, Stats> $statsMap) {
    // Sort the map by total user count.
    $statsArray = $statsMap->toArray();
    uasort($statsArray, 'Stats::compare');

    $readme = $this->repo->getFile('templates/README.md');

    $readme .= "Site | Total | MAU | DAU\n";
    $readme .= "--- | ---:| ---:| ---:\n";

    foreach ($statsArray as $s => $stats) {
      $count = $stats->count;
      $mau = $stats->mau;
      $dau = $stats->dau;
      if ($stats->valid) {
        $readme .= "[$s]({$s})|{$count}|{$mau}|{$dau}\n";
      } else {
        $readme .= "[$s]({$s})|-|-|-\n";
      }
    }

    $readme .= "\nAs of " . date("F j, Y") . ".\n";

    // Write the README.md with the latest data.
    if ($this->debugNoWrites) {
      $this->logger->addDebug("Would be writing README.md:");
      $this->logger->addDebug($readme);
    } else {
      $this->logger->addDebug("Writing README.md:");
      $this->logger->addDebug($readme);
      $this->repo->updateFile('README.md', $readme, 'The latest data.');
    }
  }
}
