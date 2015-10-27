<?hh

use LotGD\Census\Stats;
use LotGD\Census\WarriorsPage;
use LotGD\Census\WarriorsPageState;

use Monolog\Logger;
use Monolog\Handler\NullHandler;

class WarriorsPageTest extends \PHPUnit_Framework_TestCase {
  private Logger $logger;

  public function __construct() {
    parent::__construct();

    $this->logger = new Logger('test');
  }

  public function setUp() {
    parent::setUp();

    $this->logger = new Logger('test');
    $this->logger->pushHandler(new Monolog\Handler\StreamHandler('php://stderr', Logger::WARNING));
  }

  public function testLotgdNetHtml() {
    $url = __DIR__ . '/html/lotgd.net.html';
    $contents = file_get_contents($url);
    $wp = new WarriorsPage($this->logger, $url, $contents);

    $stats = new Stats();
    $stats->count = 50;
    $stats->dau = 21;
    $stats->mau = 40;

    $this->assertEquals(WarriorsPageState::Valid, $wp->state);
    $this->assertEquals(0, Stats::compare($wp->stats, $stats));
  }

  public function testLotgdDeHtml() {
    $url = __DIR__ . '/html/lotgd.de.html';
    $contents = file_get_contents($url);
    $wp = new WarriorsPage($this->logger, $url, $contents);

    $stats = new Stats();
    $stats->count = 100;
    $stats->dau = 12;
    $stats->mau = 46;

    $this->assertEquals(WarriorsPageState::Valid, $wp->state);
    $this->assertEquals(0, Stats::compare($wp->stats, $stats));
  }

  public function testDragonPrimeLotgdNetHtml() {
    $url = __DIR__ . '/html/dragonprimelogd.net.html';
    $contents = file_get_contents($url);
    $wp = new WarriorsPage($this->logger, $url, $contents);

    $stats = new Stats();
    $stats->count = 30;
    $stats->dau = 0;
    $stats->mau = 12;

    $this->assertEquals(WarriorsPageState::Valid, $wp->state);
    $this->assertEquals(0, Stats::compare($wp->stats, $stats));
  }
}
