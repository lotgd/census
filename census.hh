<?hh

require './vendor/autoload.php';

// Turn off all error reporting
error_reporting(0);

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

function updateFile(\Github\Client $client, string $path, string $content, string $commitMessage) {
  $committer = array('name' => 'Census Bot', 'email' => 'austen.mcdonald@gmail.com');
  $user = 'lotgd';
  $repo = 'census';
  $branch = 'master';
  $oldFile = $client->api('repo')->contents()->show($user, $repo, $path, $branch);
  $fileInfo = $client->api('repo')->contents()->update($user, $repo, $path, $content, $commitMessage, $oldFile['sha'], $branch, $committer);
}

$sites = Vector {};

$handle = fopen('./sites', 'r');
if ($handle) {
  while (($line = fgets($handle)) !== false) {
    $sites->add(trim($line));
  }
  fclose($handle);
} else {
  $log->addError("Cannot open sites file.");
  exit(1);
}

$statsMap = Map {};

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

$stats = new Stats();
$stats->count = 10;
$stats->mau = 5;
$stats->dau = 1;
$stats->valid = true;
$statsMap['site1'] = $stats;

$stats = new Stats();
$stats->count = 50;
$stats->mau = 3;
$stats->dau = 2;
$stats->valid = true;
$statsMap['site2'] = $stats;

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

$client = new \Github\Client();
$client->authenticate($_ENV['GITHUB_TOKEN'], null, Github\Client::AUTH_HTTP_TOKEN);

updateFile($client, 'README.md', $readme, 'The latest data.');
