<?hh

namespace LotGD\Census;

class CSV {
  private \Monolog\Logger $logger;
  private array $data;
  private string $filename;

  public function __construct(\Monolog\Logger $logger, string $filename, string $contents) {
    $this->logger = $logger;
    $this->filename = $filename;
    $lines = explode(PHP_EOL, $contents);
    $this->data = array_map(fun('str_getcsv'), $lines);

    $end = count($this->data) - 1;
    if ($end >= 0 && count($this->data[$end]) == 1 && $this->data[$end][0] == null) {
      array_pop($this->data);
    }
  }

  public function contents() : string {
    $contents = '';
    foreach ($this->data as $row) {
      $contents .= implode(",", $row) . "\n";
    }
    return $contents;
  }

  public function appendToRows(Map<string, int> $map) {
    $csv = $this->filename;
    $date = date("Y-m-d");

    // Process header...
    $i = 0;
    if (count($this->data) == 0) {
      $this->logger->addError("Couldn't find header row in {$csv}.");
      return;
    }
    $row = $this->data[$i];
    $columns = count($row);
    array_push($row, $date);
    $this->data[$i] = $row;
    $i++;

    for (; $i < count($this->data); $i++) {
      $row = $this->data[$i];
      if ($columns != count($row)) {
        $this->logger->addError("Row in {$csv} doesn't have the right number of columns.");
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
      $this->data[$i] = $row;
    }

    foreach ($map as $s => $value) {
      $row = array($s);
      for ($j = 0; $j < $columns - 1; $j++) {
        array_push($row, '');
      }
      array_push($row, $value);
      $this->data[$i] = $row;
      $i++;
    }
  }
}
