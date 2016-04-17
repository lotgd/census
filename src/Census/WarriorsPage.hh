<?hh

namespace LotGD\Census;

function DOMinnerHTML(\DOMNode $element)
{
    $innerHTML = "";
    $children  = $element->childNodes;

    foreach ($children as $child)
    {
        $innerHTML .= $element->ownerDocument->saveHTML($child);
    }

    return $innerHTML;
}

enum WarriorsPageState: int {
  Unknown = 0;
  Valid = 1;
  End = 2;
  Error = 3;
}

class WarriorsPage {
  public ?Stats $stats;
  public WarriorsPageState $state;

  private \Monolog\Logger $logger;

  public function __construct(\Monolog\Logger $logger, string $url, string $contents) {
    $this->logger = $logger;
    $this->state = WarriorsPageState::Unknown;

    // Some light preprocessing.
    $contents = str_replace('&nbsp;', ' ', $contents);

    $doc = new \DOMDocument();
    $doc->loadHTML($contents);
    $this->state = $this->computeStats($doc, $url);
  }

  private function getTimeFromString(string $t) : int {
    $max = PHP_INT_MAX;

    $t = trim($t);
    // Some cruft sometimes appears.
    $t = str_replace('.', '', $t);

    $t = str_replace('Today', '0 days', $t);
    $t = str_replace('Heute', '0 days', $t);
    $t = str_replace('Gastern', '1 days', $t);

    $t = str_replace('Never', "{$max} days", $t);
    $t = str_replace('Nie', "{$max} days", $t);

    $i = intval($t);

    // Handle some odd data, like -67242 days.
    if ($i < 0) {
      $i = PHP_INT_MAX;
    }

    return $i;
  }

  private function findHeader(\DOMDocument $doc, \DOMNodeList $trs): \DOMElement {
    $trhead = null;
    foreach ($trs as $tr) {
      if ($tr->getAttribute('class') == 'trhead') {
        $trhead = $tr;
        break;
      }
    }

    if ($trhead == null) {
      // Try to search for the first row and check if it has Last or Zuletzt.
      foreach ($trs as $tr) {
        $inner = DOMinnerHTML($tr);
        if (strpos($inner, 'Last') !== FALSE || strpos($inner, 'Zuletzt') !== FALSE) {
          $trhead = $tr;
          break;
        }
      }
    }

    return $trhead;
  }

  private function findTimeColumnIndex(\DOMElement $trhead): int {
    // Find column that contains 'Last' or 'Zuletzt'.
    $timeColumnIndex = -1;
    $currentIndex = 0;
    $node = $trhead->childNodes->item(0);
    while ($node) {
      if (strpos($node->nodeValue, 'Last') !== FALSE || strpos($node->nodeValue, 'Zuletzt') !== FALSE) {
        $timeColumnIndex = $currentIndex;
        break;
      }

      if (strlen(trim($node->nodeValue)) > 0) {
        $currentIndex++;
      }
      $node = $node->nextSibling;
    }
    return $timeColumnIndex;
  }

  private function computeStats(\DOMDocument $doc, string $url): WarriorsPageState {
    $stats = new Stats();

    if ($doc == null) {
      return WarriorsPageState::Error;
    }

    $trs = $doc->getElementsByTagName('tr');
    $trhead = $this->findHeader($doc, $trs);
    if ($trhead == null || $trhead->childNodes->length == 0) {
      $this->logger->addWarning("{$url} has no table header or it's empty.");
      return WarriorsPageState::Error;
    }

    $timeColumnIndex = $this->findTimeColumnIndex($trhead);
    if ($timeColumnIndex == -1) {
      $this->logger->addWarning("Columns in {$url} header don't contain 'Last' or 'Zuletzt'.");
      return WarriorsPageState::Error;
    }

    $tr = $trhead->nextSibling;
    if ($tr == null) {
      // This is the expected end state.
      $this->logger->addDebug("{$url} has an empty table.");
      return WarriorsPageState::End;
    }

    while ($tr !== null) {
      if ($tr->childNodes->length > 0) {
        $rawTime = $tr->childNodes->item($timeColumnIndex)->nodeValue;
        $time = $this->getTimeFromString($rawTime);

        $stats->count++;
        if ($time >= 0) {
          if ($time <= 1) {
            $stats->dau++;
          }
          if ($time <= 30) {
            $stats->mau++;
          }
        } else {
          $this->logger->addWarning("{$url} could not convert '{$rawTime}' to a time.");
          return WarriorsPageState::Error;
        }
      } else {
        $this->logger->addWarning("{$url} has an empty row.");
        return WarriorsPageState::Error;
      }

      $tr = $tr->nextSibling;
    }

    $this->stats = $stats;
    return WarriorsPageState::Valid;
  }
}
