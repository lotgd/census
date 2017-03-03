<?php

namespace LotGD\Census;

class Repository {
    private $logger;
    private $client;

    private $user = 'lotgd';
    private $repo = 'census';
    private $branch = 'master';
    private $committer = array(
        'name' => 'Census Bot',
        'email' => 'austen.mcdonald@gmail.com');

    public function __construct(\Monolog\Logger $logger) {
        $this->logger = $logger;
        $this->client = new \Github\Client();
        $this->client->authenticate(getenv('GITHUB_TOKEN'), null, \Github\Client::AUTH_HTTP_TOKEN);
    }

    public function getFile(string $path) : string {
        return $this->client->api('repo')->contents()->download($this->user, $this->repo, $path);
    }

    public function updateFile(string $path, string $content, string $commitMessage) {
        $oldFile = $this->client->api('repo')->contents()->show($this->user, $this->repo, $path, $this->branch);
        $fileInfo = $this->client->api('repo')->contents()->update($this->user, $this->repo, $path, $content, $commitMessage, $oldFile['sha'], $this->branch, $this->committer);
    }

    public function getSites() : array {
        $sites = array();

        $sitesFile = null;
        try {
            $sitesFile = $this->getFile('sites');
        } catch (Exception $e) {
            $this->logger->addError("Cannot find sites file.");
            exit(1);
        }

        if (!$sitesFile) {
            $this->logger->addError("Could not read from sites file.");
            exit(1);
        }

        $fp = fopen("php://memory", 'r+');
        fputs($fp, $sitesFile);
        rewind($fp);
        while ($line = fgets($fp)) {
            $line = trim($line);
            if (strlen($line) > 0) {
                array_push($sites, trim($line));
            }
        }
        fclose($fp);

        return $sites;
    }
}
