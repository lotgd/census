<?hh

namespace LotGD\Census;

use LotGD\Census\WarriorsPageState;

class Census {
  public bool $debugFakeData = false;
  public bool $debugLocalOnly = false;
  public Repository $repo;

  public Logger $logger;

  public function __construct(\Monolog\Logger $logger) {
    $this->logger = $logger;
    $this->repo = new Repository($this->logger);
  }

  public function readCommandLineArguments(array $argv, int $argc) {
    for ($i = 1; $i < $argc; $i++) {
      $a = $argv[$i];

      if ($a == '--local-only') {
        $this->debugLocalOnly = true;
        continue;
      }
      if ($a == '--fake-data') {
        $this->debugFakeData = true;
        continue;
      }
      if ($a == '--debug') {
        $this->logger->setHandlers(array(new \Monolog\Handler\StreamHandler('php://stderr', \Monolog\Logger::DEBUG)));
        continue;
      }

      echo "Invalid argument: {$a}", PHP_EOL;
      exit(1);
    }
  }

  public static function main(array $argv, int $argc) {
    $listURL = '/list.php?op=bypage&page=';

    $l = new \Monolog\Logger('census');
    $l->pushHandler(new \Monolog\Handler\StreamHandler('php://stderr', \Monolog\Logger::WARNING));

    $c = new Census($l);
    $c->readCommandLineArguments($argv, $argc);

    $statsMap = Map {};

    if ($c->debugFakeData) {
      $sites = Vector {};

      $stats = new Stats();
      $stats->count = 103;
      $stats->dau = 12;
      $stats->mau = 23;
      $statsMap['site3'] = $stats;

      $stats = new Stats();
      $stats->count = 123;
      $stats->dau = 3;
      $stats->mau = 13;
      $statsMap['twx.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 400;
      $stats->dau = 126;
      $stats->mau = 316;
      $statsMap['www.lotgd.net'] = $stats;

      $stats = new Stats();
      $stats->count = 594;
      $stats->dau = 33;
      $stats->mau = 135;
      $statsMap['www.dragonsofmyth.com'] = $stats;

      $stats = new Stats();
      $stats->count = 270;
      $stats->dau = 8;
      $stats->mau = 25;
      $statsMap['stormvalley.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 29;
      $stats->dau = 1;
      $stats->mau = 10;
      $statsMap['dragonprimelogd.net'] = $stats;

      $stats = new Stats();
      $stats->count = 586;
      $stats->dau = 80;
      $stats->mau = 274;
      $statsMap['forbiddenrealm.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 94;
      $stats->dau = 6;
      $stats->mau = 36;
      $statsMap['enchantedland.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 7;
      $stats->dau = 0;
      $stats->mau = 0;
      $statsMap['twx.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 30;
      $stats->dau = 3;
      $stats->mau = 7;
      $statsMap['ess.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 395;
      $stats->dau = 66;
      $stats->mau = 214;
      $statsMap['lotgd4adults2.com'] = $stats;

      $stats = new Stats();
      $stats->count = 43;
      $stats->dau = 19;
      $stats->mau = 43;
      $statsMap['deathstar.rpglink.in'] = $stats;

      $stats = new Stats();
      $stats->count = 20;
      $stats->dau = 0;
      $stats->mau = 6;
      $statsMap['golden-empire.com'] = $stats;

      $stats = new Stats();
      $stats->count = 632;
      $stats->dau = 191;
      $stats->mau = 444;
      $statsMap['the-complex.net'] = $stats;

      $stats = new Stats();
      $stats->count = 54;
      $stats->dau = 12;
      $stats->mau = 38;
      $statsMap['tynastera2.com'] = $stats;

      $stats = new Stats();
      $stats->count = 1494;
      $stats->dau = 308;
      $stats->mau = 705;
      $statsMap['www.lotgd.de'] = $stats;
    } else {
      $sites = $c->repo->getSites();

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

    $c->logger->addDebug('Sites data:');
    $c->logger->addDebug(var_export($statsMap, true));

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
    $localFilePath = __DIR__ . '/../../' . $file;
    if ($this->debugLocalOnly) {
      $contents = file_get_contents($localFilePath);
    } else {
      $contents = $this->repo->getFile($file);
    }

    if ($contents === null) {
      $this->logger->addError("Couldn't get {file} from the repo.");
      return;
    }

    $csv = new CSV($this->logger, $file, $contents);
    $csv->appendToRows($map);
    $result = $csv->contents();

    $this->logger->addDebug("Writing {$file}:");
    $this->logger->addDebug($result);
    if ($this->debugLocalOnly) {
      file_put_contents($localFilePath, $result);
    } else {
      $this->repo->updateFile($file, $contents, 'The latest data.');
    }
  }

  private function updateReadme(Map<string, Stats> $statsMap) {
    $localTemplateFilePath = __DIR__ . '/../../templates/README.md';
    $localReadmeFilePath = __DIR__ . '/../../README.md';

    // Sort the map by total user count.
    $statsArray = $statsMap->toArray();
    uasort($statsArray, 'Stats::compare');

    if ($this->debugLocalOnly) {
      $readme = file_get_contents($localTemplateFilePath);
    } else {
      $readme = $this->repo->getFile('templates/README.md');
    }

    $readme .= "Site | Total | MAU | DAU\n";
    $readme .= "--- | ---:| ---:| ---:\n";

    foreach ($statsArray as $s => $stats) {
      $count = $stats->count;
      $mau = $stats->mau;
      $dau = $stats->dau;
      $readme .= "[$s](http://{$s})|{$count}|{$mau}|{$dau}\n";
    }

    $readme .= "\nAs of " . date("F j, Y") . ".\n";

    // Write the README.md with the latest data.
    $this->logger->addDebug("Writing README.md:");
    $this->logger->addDebug($readme);
    if ($this->debugLocalOnly) {
      file_put_contents($localReadmeFilePath, $readme);
    } else {
      $this->repo->updateFile('README.md', $readme, 'The latest data.');
    }
  }
}
