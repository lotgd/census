<?hh

require './vendor/autoload.php';

// Only report errors.
error_reporting(E_ERROR | E_PARSE);

use Monolog\Logger;
use Monolog\Handler\StreamHandler;

$log = new Logger('census');
$log->pushHandler(new StreamHandler('php://stderr', Logger::WARNING));

$listURL = '/list.php?page=';

class Stats {
  public int $count = 0;
  public int $dau = 0;
  public int $mau = 0;
  public bool $valid = false;
}

function statsCmp(Stats $a, Stats $b) : int {
  return $b->count - $a->count;
}

function getTimeFromString(string $t) : int {
  $t = trim($t);
  str_replace('Today', '0 days', $t);

  return intval($t);
}

function collectStats(Logger $log, string $url, DOMDocument $doc, Stats $stats) : bool {
  if ($doc == null) {
    return false;
  }

  $trs = $doc->getElementsByTagName('tr');
  $trhead = null;
  foreach ($trs as $tr) {
    if ($tr->getAttribute('class') == 'trhead') {
      $trhead = $tr;
      break;
    }
  }

  if ($trhead == null || $trhead->childNodes->length == 0) {
    $log->addWarning("{$url} has no <tr class='trhead'> or it's empty.");
    return false;
  }
  $lastColumn = $trhead->childNodes->item($trhead->childNodes->length - 1);
  if (strpos($lastColumn->nodeValue, 'Last') === false) {
    $log->addWarning("{$url} last column in header doesn't contain 'Last'.");
    return false;
  }

  $tr = $trhead->nextSibling;
  if ($tr == null) {
    // This is the expected end state.
    $log->addDebug("{$url} has an empty table.");
    return false;
  }

  while ($tr !== null) {
    if ($tr->childNodes->length > 0) {
      $rawTime = $tr->childNodes->item($tr->childNodes->length - 1)->nodeValue;
      $time = getTimeFromString($rawTime);

      $stats->count++;
      if ($time >= 0) {
        if ($time <= 1) {
          $stats->dau++;
        }
        if ($time <= 30) {
          $stats->mau++;
        }
      } else {
        $log->addWarning("{$url} could not convert '{$rawTime}' to a time.");
        return false;
      }
    } else {
      $log->addWarning("{$url} has an empty row.");
      return false;
    }

    $tr = $tr->nextSibling;
  }
  return true;
}

function getRepoFile(\Github\Client $client, string $path) : string {
  $user = 'lotgd';
  $repo = 'census';
  return $client->api('repo')->contents()->download($user, $repo, $path);
}

function updateFile(\Github\Client $client, string $path, string $content, string $commitMessage) {
  $committer = array('name' => 'Census Bot', 'email' => 'austen.mcdonald@gmail.com');
  $user = 'lotgd';
  $repo = 'census';
  $branch = 'master';
  $oldFile = $client->api('repo')->contents()->show($user, $repo, $path, $branch);
  $fileInfo = $client->api('repo')->contents()->update($user, $repo, $path, $content, $commitMessage, $oldFile['sha'], $branch, $committer);
}

$client = new \Github\Client();
$client->authenticate($_ENV['GITHUB_TOKEN'], null, Github\Client::AUTH_HTTP_TOKEN);

$sites = Vector {};

try {
  $sitesFile = getRepoFile($client, 'sites');
} catch (Exception $e) {
  $log->addError("Cannot find sites file.");
  exit(1);
}

if (!$sitesFile) {
  $log->addError("Could not read from sites file.");
  exit(1);
}

$fp = fopen("php://memory", 'r+');
fputs($fp, $sitesFile);
rewind($fp);
while ($line = fgets($fp)) {
  $line = trim($line);
  if (strlen($line) > 0) {
    $sites->add(trim($line));
  }
}
fclose($fp);

$statsMap = Map {};

if ($argc >= 2) {
  if ($argv[1] == '--fake-data') {
    $sites = Vector {};

    $stats = new Stats();
    $stats->count = 0;
    $stats->dau = 0;
    $stats->mau = 0;
    $stats->valid = true;
    $statsMap['twx.rpglink.in'] = $stats;

    $stats = new Stats();
    $stats->count = 103;
    $stats->dau = 12;
    $stats->mau = 23;
    $stats->valid = true;
    $statsMap['site3'] = $stats;
  }
}

foreach ($sites as $s) {
  $doc = new DOMDocument();

  $stats = new Stats();
  $i = 1;
  while ($contents = file_get_contents($url = 'http://' . $s . $listURL . $i)) {
    $log->addDebug("Fetching {$url}");
    $doc->loadHTML($contents);
    $result = collectStats($log, $url, $doc, $stats);

    $i++;

    if ($i > 100 || !$result) {
      $log->addDebug("  done or error");
      break;
    } else {
      $stats->valid = true;
    }
  }

  $statsMap[$s] = $stats;
}

// Sort the map by total user count.
$statsArray = $statsMap->toArray();
uasort($statsArray, 'statsCmp');

$readme = file_get_contents('templates/README.md');

$readme .= "Site | Total | MAU | DAU\n";
$readme .= "--- | ---:| ---:| ---:\n";

foreach ($statsArray as $s => $stats) {
  $count = $stats->count;
  $mau = $stats->mau;
  $dau = $stats->dau;
  if ($stats->valid) {
    $readme .= "{$s}|{$count}|{$mau}|{$dau}\n";
  } else {
    $readme .= "{$s}|-|-|-\n";
  }
}

$readme .= "\nAs of " . date("F j, Y") . ".\n";

// Write the README.md with the latest data.
updateFile($client, 'README.md', $readme, 'The latest data.');

function updateCSV(Logger $log, \Github\Client $client, string $csv, Map<string, int> $map) {
  $date = date("Y-m-d");
  $file = "";
  $contents = getRepoFile($client, $csv);
  if ($contents === null) {
    $log->addError("Couldn't get {$csv} from the repo.");
    return;
  }
  $lines = explode(PHP_EOL, $contents);
  $data = array_map('str_getcsv', $lines);

  // Process header...
  $i = 0;
  if (count($data) == 0) {
    $log->addError("Couldn't find header row in {$csv}.");
    return;
  }
  $row = $data[$i];
  $columns = count($row);
  array_push($row, $date);
  $file .= implode(",", $row) . "\n";
  $i++;

  for (; $i < count($data) - 1; $i++) { // skip the last row
    $row = $data[$i];
    if ($columns != count($row)) {
      $log->addError("Row in {$csv} doesn't have the right number of columns.");
      return;
    }
    $s = $row[0];
    if ($map->containsKey($s)) {
      $value = $map[$s];
      array_push($row, $value);
      $map->remove($s);
    } else {
      array_push($row, '');
    }
    $file .= implode(",", $row) . "\n";
  }

  foreach ($map as $s => $value) {
    $row = array($s, '');
    for ($i = 0; $i < $columns - 2; $i++) {
      array_push($row, '');
    }
    array_push($row, $value);
    $file .= implode(",", $row) . "\n";
  }

  updateFile($client, $csv, $file, 'The latest data.');
}

// Now, read in CSVs and add to them.
updateCSV($log, $client, 'data/total.csv', $statsMap->map(function (Stats $s) : int {
  return $s->count;
}));
updateCSV($log, $client, 'data/mau.csv', $statsMap->map(function (Stats $s) : int {
  return $s->mau;
}));
updateCSV($log, $client, 'data/dau.csv', $statsMap->map(function (Stats $s) : int {
  return $s->dau;
}));
