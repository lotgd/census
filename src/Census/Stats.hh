<?hh

namespace LotGD\Census;

class Stats {
  public int $count = 0;
  public int $dau = 0;
  public int $mau = 0;

  public function add(Stats $s) {
    $this->count += $s->count;
    $this->dau += $s->dau;
    $this->mau += $s->mau;
  }

  public static function compare(Stats $a, Stats $b) : int {
    return $b->count - $a->count;
  }
}
