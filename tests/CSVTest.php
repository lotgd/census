<?hh

use LotGD\Census\CSV;

use Monolog\Logger;
use Monolog\Handler\NullHandler;

class CSVTest extends \PHPUnit_Framework_TestCase {
  private Logger $logger;

  public function __construct() {
    parent::__construct();

    $this->logger = new Logger('test');
  }

  public function setUp() {
    parent::setUp();

    $this->logger = new Logger('test');
    $this->logger->pushHandler(new NullHandler());
  }

  public function testReadBack() {
    $contents = "";
    $csv = new CSV($this->logger, 'filename', $contents);
    $this->assertEquals($contents, $csv->contents());

    $contents = "a,b\nc,d\n";
    $csv = new CSV($this->logger, 'filename', $contents);
    $this->assertEquals($contents, $csv->contents());
  }

  public function testAppendToRows() {
    $contents = "a,b\ns1,d\n";
    $date = date("Y-m-d");
    $expectedContents = "a,b,{$date}\ns1,d,1\ns2,,2\n";

    $csv = new CSV($this->logger, 'filename', $contents);

    $csv->appendToRows(Map {
      's1' => 1,
      's2' => 2,
    });

    $this->assertEquals($expectedContents, $csv->contents());
  }
}
