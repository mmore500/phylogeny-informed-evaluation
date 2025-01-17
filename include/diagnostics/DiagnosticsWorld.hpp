/**
 * Code adapted from https://github.com/jgh9094/ECJ-2022-suite-of-diagnostics-for-selection-schemes/ by Jose Hernandez
*/
#pragma once

#include <iostream>
#include <algorithm>
#include <functional>
#include <sys/stat.h>

#include "emp/base/vector.hpp"
#include "emp/base/Ptr.hpp"
#include "emp/Evolve/World.hpp"

#include "phylogeny/phylogeny_utils.hpp"
#include "selection/SelectionSchemes.hpp"
#include "utility/printing.hpp"

#include "DiagnosticsConfig.hpp"
#include "DiagnosticsOrg.hpp"
#include "DiagnosticsProblems.hpp"

// TODO - snapshot config
namespace diag {

class DiagnosticsWorld : public emp::World<DiagnosticsOrg> {
public:
  using base_t = emp::World<DiagnosticsOrg>;
  using org_t = DiagnosticsOrg;
  using genome_t = typename org_t::genome_t;
  using phenotype_t = typename org_t::phenotype_t;

  using systematics_t = emp::Systematics<org_t, genome_t, phylo::phenotype_info>;
  using taxon_info_t = phylo::phenotype_info;
  using taxon_t = typename systematics_t::taxon_t;

  using config_t = DiagnosticsConfig;
  using selection_fun_t = std::function< emp::vector<size_t>&(size_t, const emp::vector<size_t>&, const emp::vector<size_t>&) >;

  struct Grouping {
    size_t group_id = 0;
    size_t group_size = 0;
    emp::vector<size_t> member_ids;

    void Resize(size_t size, size_t value=0) {
      group_size = size;
      member_ids.resize(group_size, value);
    }

    size_t GetSize() const { return member_ids.size(); }
  };

  // TODO - setup statistics info struct that gets computed when it needs to
  struct SelectedStatistics {

    size_t num_unique_cand_selected;
    double entropy_cand_selected;
    size_t parents_num_tests_covered;
    emp::vector<bool> parent_test_coverage;
    // true pop coverage vs selected coverage

    void Reset() {
      parent_test_coverage.clear();
      parents_num_tests_covered = 0;
      num_unique_cand_selected = 0;
      entropy_cand_selected = 0;
    }

    void Calculate(
      const emp::vector<size_t>& selected,
      DiagnosticsWorld& world
    ) {
      Reset();
      num_unique_cand_selected = emp::UniqueCount(selected);
      entropy_cand_selected = emp::ShannonEntropy(selected);

      // Note, this is not the cleanest way to track this... should think about better implementations
      size_t num_tests = world.GetConfig().DIAGNOSTIC_DIMENSIONALITY();
      emp::vector<bool> parent_test_coverage(num_tests, false);
      for (size_t test_id = 0; test_id < num_tests; ++test_id) {
        for (size_t s_i = 0; (s_i < selected.size()) && !parent_test_coverage[test_id]; ++s_i) {
          const size_t org_id = selected[s_i];
          auto& org = world.GetOrg(org_id);
          parent_test_coverage[test_id] = parent_test_coverage[test_id] || org.IsGeneOptimal(test_id);
        }
      }

      parents_num_tests_covered = std::accumulate(
        parent_test_coverage.begin(),
        parent_test_coverage.end(),
        0
      );
    }

  };

protected:
  const config_t& config;

  emp::Ptr<BaseDiagnostic> base_diagnostic=nullptr; ///< Base-layer diagnostic to use to translate genomes to phenotypes
  MultiValleyCrossingDiagnostic valley_diagnostic;  ///< If in use, it's layered on top of another diagnostic (base_diagnostic).

  emp::Signal<void(size_t)> do_org_evaluation_sig;
  std::function<void(const genome_t&, phenotype_t&)> translate_genome_fun;

  std::function<bool(void)> stop_run;

  // TODO - create a class/struct that manages all of this?
  size_t total_tests=0;
  emp::vector<double> org_aggregate_scores;
  emp::vector<bool> pop_test_coverage;

  emp::vector< emp::vector< std::function<double(void)> > > fit_fun_set; ///< Per-organism, per-test
  emp::vector< std::function<double(void)> > agg_score_fun_set; ///< Per-organism, aggregate score

  emp::vector< emp::vector<double> > org_test_scores;   ///< Test scores for each organism
  emp::vector< emp::vector<bool> > org_test_evaluations; ///< Which test cases has each organism been evaluated on?
  emp::vector< emp::vector<bool> > org_test_estimations; ///< Which test cases have been estimated?

  emp::vector<size_t> possible_test_ids;
  emp::vector<size_t> possible_pop_ids;

  emp::vector<Grouping> test_groupings; ///< Groupings of tests (needs to be same size as org_groupings)
  emp::vector<Grouping> org_groupings;  ///< Groupings of organisms (needs to be same size as test_groupings)
  emp::vector<size_t> org_group_ids;    ///< Group ID for each organism

  std::function<void(void)> assign_test_groupings;
  std::function<void(void)> assign_org_groupings;

  std::function<double(size_t, size_t)> estimate_test_score;

  emp::Ptr<selection::BaseSelect> selector;
  emp::vector<size_t> selected_parent_ids;

  std::function<void(void)> run_selection_routine;
  selection_fun_t selection_fun;

  emp::Ptr<systematics_t> systematics_ptr;

  size_t total_test_evaluations = 0;  ///< Tracks total number of "test case" evaluations (across all organisms since beginning of run)
  size_t total_test_estimations = 0;
  std::string output_dir;

  size_t true_max_fit_org_id = 0;     ///< Tracks max fit organism (based on 'true' aggregate fitness)
  SelectedStatistics selection_stats;

  // -- data files --
  emp::Ptr<emp::DataFile> summary_file_ptr;
  emp::Ptr<emp::DataFile> phylodiversity_file_ptr;
  emp::Ptr<emp::DataFile> elite_file_ptr;

  void Setup();
  void SetupDiagnostic();
  void SetupSelection();
  void SetupEvaluation();
  void SetupFitFunEstimator();
  void SetupMutator();
  void SetupPhylogenyTracking();
  void SetupDataCollection();
  void SetupStoppingCondition();

  template<typename DIAG_PROB>
  void SetupDiagnosticHelper();

  void SetupFitFunEstimator_None();
  void SetupFitFunEstimator_Ancestor();
  void SetupFitFunEstimator_AncestorOpt();
  void SetupFitFunEstimator_Relative();
  void SetupFitFunEstimator_RelativeOpt();

  void SetupEvaluation_Cohort();
  void SetupEvaluation_DownSample();
  void SetupEvaluation_Full();

  void SetupSelection_Lexicase();
  void SetupSelection_Tournament();
  void SetupSelection_Truncation();
  void SetupSelection_None();
  void SetupSelection_Random();

  void SetupStoppingCondition_Generations();
  void SetupStoppingCondition_Evaluations();

  void InitializePopulation();

  void DoEvaluation();
  void DoSelection();
  void DoUpdate();

  void SnapshotConfig();
  // void DoPopSnapshot();

public:

  DiagnosticsWorld(
    const DiagnosticsConfig& in_config
  ) :
    base_t("DiagnosticsWorld", false),
    config(in_config)
  {
    NewRandom(config.SEED()); // Set the seed in the world's random number generator.
    Setup();
  }

  ~DiagnosticsWorld() {
    if (base_diagnostic != nullptr) base_diagnostic.Delete();
    if (selector != nullptr) selector.Delete();
    if (summary_file_ptr != nullptr) summary_file_ptr.Delete();
    if (elite_file_ptr != nullptr) elite_file_ptr.Delete();
    if (phylodiversity_file_ptr != nullptr) phylodiversity_file_ptr.Delete();
  }

  void RunStep();
  void Run();

  const config_t& GetConfig() const { return config; }
};

void DiagnosticsWorld::RunStep() {
  DoEvaluation();
  DoSelection();
  DoUpdate();
}

void DiagnosticsWorld::Run() {
  while (!stop_run()) {
    RunStep();
  }
}

void DiagnosticsWorld::DoEvaluation() {
  emp_assert(org_aggregate_scores.size() == config.POP_SIZE());
  emp_assert(fit_fun_set.size() == config.POP_SIZE());
  emp_assert(org_test_scores.size() == config.POP_SIZE());
  emp_assert(org_test_evaluations.size() == config.POP_SIZE());

  // Reset current true max fitness organism
  true_max_fit_org_id = 0;
  std::fill(
    pop_test_coverage.begin(),
    pop_test_coverage.end(),
    false
  );
  // Assign test groupings (if any)
  assign_test_groupings();
  // Assign organism groupings (if any)
  assign_org_groupings();
  // Evaluate each organism
  for (size_t org_id = 0; org_id < GetSize(); ++org_id) {
    // Evaluation signal actions will:
    // - Reset relevant phenotype tracking info
    // - Translate organism genomes
    // - Update test scores
    // - Record taxon information
    // - Update aggregate scores (using estimators)
    do_org_evaluation_sig.Trigger(org_id);
    // std::cout << "--" << std::endl;
    // std::cout << org_test_evaluations[org_id] << std::endl;
    // std::cout << org_test_scores[org_id] << std::endl;
    // std::cout << org.GetPhenotype() << std::endl;
  }
  // Estimate aggregate fitness for each organism
  // Need to do this separately to ensure all possible descendants of current taxa have been evaluated
  // Otherwise, estimation can crash
  for (size_t org_id = 0; org_id < GetSize(); ++org_id) {
    double est_agg_score = 0.0;
    for (size_t test_id = 0; test_id < total_tests; ++test_id) {
      est_agg_score += fit_fun_set[org_id][test_id]();
    }
    org_aggregate_scores[org_id] = est_agg_score;
  }
}

void DiagnosticsWorld::DoSelection() {
  // Run selection-specific routine
  run_selection_routine();
  emp_assert(selected_parent_ids.size() == config.POP_SIZE());
  // each selected parent id reproduces
  for (size_t id : selected_parent_ids) {
    DoBirth(GetGenomeAt(id), id);
  }
}

void DiagnosticsWorld::DoUpdate() {
  // (1) Compute any per-generation statistics?
  emp_assert(config.PRINT_INTERVAL() > 0);
  const size_t cur_update = GetUpdate();
  const bool final_update = cur_update == config.MAX_GENS();
  const bool print_interval = !(cur_update % config.PRINT_INTERVAL()) || final_update;
  const bool summary_data_interval = !(cur_update % config.OUTPUT_SUMMARY_DATA_INTERVAL()) || final_update;
  const bool snapshot_interval = !(cur_update % config.SNAPSHOT_INTERVAL()) || final_update;

  // Output to summary data file?
  if (summary_data_interval) {
    // Update selection statistics
    selection_stats.Calculate(selected_parent_ids, *this);
    summary_file_ptr->Update();
    elite_file_ptr->Update();
    phylodiversity_file_ptr->Update();
  }

  if (snapshot_interval) {
    systematics_ptr->Snapshot(output_dir + "phylo_" + emp::to_string(GetUpdate()) + ".csv");
  }

  // Print status?
  if ( print_interval ) {
    std::cout << "update: " << GetUpdate() << "; ";
    std::cout << "best score (" << true_max_fit_org_id << "): " << GetOrg(true_max_fit_org_id).GetAggregateScore();
    std::cout << std::endl;
  }

  Update();
}


void DiagnosticsWorld::Setup() {
  std::cout << "--- Setting up DiagnosticsWorld ---" << std::endl;

  // Reset the world
  Reset();
  total_test_evaluations = 0;

  // Setup population structure
  SetPopStruct_Mixed(true);

  // Configure world to set organism ID on placement
  OnPlacement(
    [this](size_t pos) {
      auto& org = GetOrg(pos);
      org.SetPopID(pos);
      emp_assert(org.GetPopID() == pos);
    }
  );

  // Setup diagnostic problem
  SetupDiagnostic();

  // Setup evaluation
  SetupEvaluation();

  // Configure selection
  SetupSelection();

  // Setup fitness function estimator
  SetupFitFunEstimator();

  // Setup mutation function
  SetupMutator();

  // Setup stopping condition
  SetupStoppingCondition();

  // Setup data collection
  SetupDataCollection();

  // Initialize population
  InitializePopulation();

  // Configure world to automatically handle mutations
  SetAutoMutate();

  // Output snapshot of run configuration
  SnapshotConfig();
}

void DiagnosticsWorld::SetupDiagnostic() {
  std::cout << "Configuring diagnostic: " << config.DIAGNOSTIC() << std::endl;

  if (config.DIAGNOSTIC() == "exploitation-rate") {
    // SetupDiagnostic_ExploitationRate();
    SetupDiagnosticHelper<ExploitationRateDiagnostic>();
  } else if (config.DIAGNOSTIC() == "ordered-exploitation") {
    // SetupDiagnostic_OrderedExploitation();
    SetupDiagnosticHelper<OrderedExploitationDiagnostic>();
  } else if (config.DIAGNOSTIC() == "contradictory-objectives") {
    // SetupDiagnostic_ContradictoryObjectives();
    SetupDiagnosticHelper<ContradictoryObjectivesDiagnostic>();
  } else if (config.DIAGNOSTIC() == "multipath-exploration") {
    // SetupDiagnostic_MultipathExploration();
    SetupDiagnosticHelper<MultiPathExplorationDiagnostic>();
  } else {
    std::cout << "ERROR: UNKNOWN DIAGNOSTIC, " << config.DIAGNOSTIC() << std::endl;
    exit(-1);
  }
}

template<typename DIAG_PROB>
void DiagnosticsWorld::SetupDiagnosticHelper() {
  base_diagnostic = emp::NewPtr<DIAG_PROB>(
    config.TARGET(),
    config.CREDIT()
  );

  // Configure genome translation
  if (config.VALLEY_CROSSING()) {
    translate_genome_fun = [this](const genome_t& genome, phenotype_t& phen) {
      auto& diag = *(base_diagnostic.Cast<DIAG_PROB>());
      // Translate according to base diagnostic
      diag.Translate(genome, phen);
      // std::cout << " 0-> " << phen << std::endl;
      // Apply valley crossing over phenotype
      phenotype_t valley_phen = valley_diagnostic.Translate(phen);
      // std::cout << " 1-> " << valley_phen << std::endl;
      emp_assert(valley_phen.size() == phen.size());
      std::copy(
        valley_phen.begin(),
        valley_phen.end(),
        phen.begin()
      );
    };
  } else {
    translate_genome_fun = [this](const genome_t& genome, phenotype_t& phen) {
      auto& diag = *(base_diagnostic.Cast<DIAG_PROB>());
      diag.Translate(genome, phen);
    };
  }

}

void DiagnosticsWorld::SetupMutator() {
  std::cout << "Configuring mutator" << std::endl;
  // TODO - Need to set things up to automutate?
  SetMutFun(
    [this](org_t& org, emp::Random& random) {
      // number of mutations and solution genome
      size_t mcnt = 0;
      genome_t& genome = org.GetGenome();

      // quick checks
      emp_assert(genome.size() == config.DIAGNOSTIC_DIMENSIONALITY());
      emp_assert(config.TARGET() > 0);

      for (size_t i = 0; i < genome.size(); ++i) {
        // if we do a mutation at this objective
        if (random.P(config.MUTATE_PER_SITE_RATE())) {
          const double mut = random.GetRandNormal(config.MUTATE_MEAN(), config.MUTATE_STD());
          if ( config.TARGET() < (genome[i] + mut) ) {
            // Rebound
            genome[i] = config.TARGET() - (genome[i] + mut - config.TARGET());
          } else if ( (genome[i] + mut) < config.GENE_LOWER_BND() ) {
            // Rebound
            genome[i] = std::abs(genome[i] + mut) + config.GENE_LOWER_BND();
          } else {
            // Add mutation
            genome[i] = genome[i] + mut;
          }
          ++mcnt;
        }
      }
      return mcnt;
    }
  );
}

void DiagnosticsWorld::SetupDataCollection() {
  std::cout << "Configure data tracking" << std::endl;
  // Configure output directory path, create directory
  output_dir = config.OUTPUT_DIR();
  mkdir(output_dir.c_str(), ACCESSPERMS);
  if(output_dir.back() != '/') {
      output_dir += '/';
  }

  // Configure phylogeny tracking
  SetupPhylogenyTracking();
  emp_assert(systematics_ptr != nullptr);
  // Create phylodiversity file
  phylodiversity_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "phylodiversity.csv"
  );
  // Create summary file
  summary_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "summary.csv"
  );
  // Create elite file
  elite_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "elite.csv"
  );

  // Configure phylodiversity file
  phylodiversity_file_ptr->AddVar(update, "update", "Generation");
  phylodiversity_file_ptr->AddVar(total_test_evaluations, "evaluations", "Test evaluations so far");
  phylodiversity_file_ptr->AddStats(*systematics_ptr->GetDataNode("evolutionary_distinctiveness") , "genotype_evolutionary_distinctiveness", "evolutionary distinctiveness for a single update", true, true);
  phylodiversity_file_ptr->AddStats(*systematics_ptr->GetDataNode("pairwise_distance"), "genotype_pairwise_distance", "pairwise distance for a single update", true, true);
  phylodiversity_file_ptr->AddCurrent(*systematics_ptr->GetDataNode("phylogenetic_diversity"), "genotype_current_phylogenetic_diversity", "current phylogenetic_diversity", true, true);
  phylodiversity_file_ptr->PrintHeaderKeys();

  // Configure summary file
  summary_file_ptr->AddVar(update, "update", "Generation");
  summary_file_ptr->AddVar(total_test_evaluations, "evaluations", "Test evaluations so far");
  summary_file_ptr->AddVar(total_test_estimations, "trait_estimations", "Test estimations so far");
  // population-wide trait coverage
  summary_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      return std::accumulate(
        pop_test_coverage.begin(),
        pop_test_coverage.end(),
        0
      );
    },
    "pop_optimal_trait_coverage",
    "True population-wide optimal trait coverage"
  );
  summary_file_ptr->AddFun<double>(
    [this]() -> double {
      return GetOrg(true_max_fit_org_id).GetAggregateScore();
    },
    "max_agg_score",
    "True maximum aggregate score"
  );

  // Selection statistics
  // num_unique_cand_selected
  summary_file_ptr->AddVar(
    selection_stats.num_unique_cand_selected,
    "num_unique_selected",
    "Number of unique candidates selected to be parents"
  );
  // entropy_cand_selected
  summary_file_ptr->AddVar(
    selection_stats.entropy_cand_selected,
    "entropy_selected_ids",
    "Entropy of candidate IDs selected"
  );
  // parents_num_tests_covered -- parents_optimal_trait_coverage
  summary_file_ptr->AddVar(
    selection_stats.parents_num_tests_covered,
    "parents_optimal_trait_coverage",
    "Number of optimal traits across all parents selected"
  );
  // optimal_trait_coverage_loss
  summary_file_ptr->AddFun<size_t>(
    [this]() {
      const size_t pop_cov = std::accumulate(
        pop_test_coverage.begin(),
        pop_test_coverage.end(),
        0
      );
      return pop_cov - selection_stats.parents_num_tests_covered;
    },
    "optimal_trait_coverage_loss",
    "(true) Pop test coverage - (true) parent test coverage"
  );

  summary_file_ptr->PrintHeaderKeys();

  // TODO - more statistics:
  // - avg dist of trait estimation
  // - max dist of trait estimation
  // - failed to find trait estimation

  // Configure elite file
  elite_file_ptr->AddVar(update, "update", "Generation");
  elite_file_ptr->AddVar(total_test_evaluations, "evaluations", "Test evaluations so far");
  // genome
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      auto& org = GetOrg(true_max_fit_org_id);
      ss << "\"";
      utils::PrintVector(ss, org.GetGenome());
      ss << "\"";
      return ss.str();
    },
    "genome",
    "elite organism genome"
  );

  // true phenotype
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      auto& org = GetOrg(true_max_fit_org_id);
      ss << "\"";
      utils::PrintVector(ss, org.GetPhenotype());
      ss << "\"";
      return ss.str();
    },
    "true_phenotype",
    "elite organism true phenotype"
  );
  // true agg score
  elite_file_ptr->AddFun<double>(
    [this]() -> double {
      auto& org = GetOrg(true_max_fit_org_id);
      return org.GetAggregateScore();
    },
    "true_agg_score",
    "elite organism true aggregate score"
  );
  // evaluated
//   org_aggregate_scores
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      ss << "\"";
      utils::PrintVector(ss, org_test_evaluations[true_max_fit_org_id]);
      ss << "\"";
      return ss.str();
    },
    "evaluated_tests",
    "test evaluations for elite organism"
  );
  // estimated phenotype
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      ss << "\"";
      utils::PrintVector(ss, org_test_scores[true_max_fit_org_id]);
      ss << "\"";
      return ss.str();
    },
    "evaluated_phenotype",
    "evaluated phenotype for elite organism"
  );
  // estimated agg score
  elite_file_ptr->AddFun<double>(
    [this]() -> double {
      return org_aggregate_scores[true_max_fit_org_id];
    },
    "evaluated_agg_score",
    "elite organism evaluated aggregate score"
  );
  // estimated - true agg
  elite_file_ptr->AddFun<double>(
    [this]() -> double {
      auto& org = GetOrg(true_max_fit_org_id);
      return org_aggregate_scores[true_max_fit_org_id] - org.GetAggregateScore();
    },
    "eval_true_agg_score_diff",
    "evaluated aggregate score - true aggregate score"
  );
  elite_file_ptr->PrintHeaderKeys();
}

void DiagnosticsWorld::SetupPhylogenyTracking() {
  std::cout << "Configure phylogeny tracking" << std::endl;
  emp_assert(systematics_ptr == nullptr);

  // Create new systematics tracker
  systematics_ptr = emp::NewPtr<systematics_t>(
    [](const org_t& org) { return org.GetGenome(); }
  );

  // Add phylo snapshot functions
  systematics_ptr->AddSnapshotFun(
    [](const taxon_t& taxon) {
      std::stringstream ss;
      ss << taxon.GetData().GetFitness();
      return ss.str();
      // return emp::to_string(taxon.GetData().GetFitness());
    },
    "fitness"
  );
  systematics_ptr->AddSnapshotFun(
    [](const taxon_t& taxon) {
      std::stringstream ss;
      ss << "\"";
      utils::PrintVector(ss, taxon.GetData().GetPhenotype());
      ss << "\"";
      return ss.str();
    },
    "phenotype"
  );
  systematics_ptr->AddSnapshotFun(
    [](const taxon_t& taxon) {
      std::stringstream ss;
      ss << "\"";
      utils::PrintVector(ss, taxon.GetInfo());
      ss << "\"";
      return ss.str();
    },
    "genome"
  );

  // *what population members are having the phylogenetic approximation applied,
  // *what taxon is being used for the approximation, and
  // *how far they are apart phylogeneticallly.

  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) {
      std::stringstream ss;
      utils::PrintVector(ss, taxon.GetData().traits_evaluated, true);
      return ss.str();
    },
    "traits_evaluated"
  );

  // -- taxon estimation information --
  // Attempted estimation
  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      if (taxon.GetData().GetPhenotype().size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      emp::vector<bool> estimate_attempts(total_tests, false);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        estimate_attempts[test_id] = taxon.GetData().GetTraitEstimationInfo(test_id).estimated;
      }
      utils::PrintVector(ss, estimate_attempts, true);
      return ss.str();
    },
    "traits_attempted_estimations"
  );

  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      if (taxon.GetData().GetPhenotype().size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      emp::vector<bool> trait_estimated(total_tests, false);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        trait_estimated[test_id] = taxon.GetData().GetTraitEstimationInfo(test_id).estimate_success;
      }
      utils::PrintVector(ss, trait_estimated, true);
      return ss.str();
    },
    "traits_successful_estimations"
  );

  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      if (taxon.GetData().GetPhenotype().size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      emp::vector<size_t> estimate_source_ids(total_tests, 0);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        estimate_source_ids[test_id] = taxon.GetData().GetTraitEstimationInfo(test_id).source_taxon_id;
      }
      utils::PrintVector(ss, estimate_source_ids, true);
      return ss.str();
    },
    "traits_estimation_source_ids"
  );

  // Estimation distances
  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      if (taxon.GetData().GetPhenotype().size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      emp::vector<size_t> est_dists(total_tests, 0);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        est_dists[test_id] = taxon.GetData().GetTraitEstimationInfo(test_id).estimation_dist;
      }
      utils::PrintVector(ss, est_dists, true);
      return ss.str();
    },
    "traits_estimation_dist"
  );

  // Estimation scores
  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      if (taxon.GetData().GetPhenotype().size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      emp::vector<double> est_scores(total_tests, 0.0);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        est_scores[test_id] = taxon.GetData().GetTraitEstimationInfo(test_id).estimated_score;
      }
      utils::PrintVector(ss, est_scores, true);
      return ss.str();
    },
    "traits_estimated_scores"
  );

  systematics_ptr->AddEvolutionaryDistinctivenessDataNode();
  systematics_ptr->AddPairwiseDistanceDataNode();
  systematics_ptr->AddPhylogeneticDiversityDataNode();

  // Note, base class takes ownership of this pointer
  AddSystematics(systematics_ptr, "genotype");
  SetupSystematicsFile(
    "genotype",
    output_dir + "systematics.csv"
  ).SetTimingRepeat(config.OUTPUT_SUMMARY_DATA_INTERVAL());


}

void DiagnosticsWorld::SetupStoppingCondition() {
  std::cout << "Configurint stopping condition" << std::endl;

  if (config.STOP_MODE() == "generations") {
    SetupStoppingCondition_Generations();
  } else if (config.STOP_MODE() == "evaluations") {
    SetupStoppingCondition_Evaluations();
  } else {
    std::cout << "Unknown STOP_MODE: " << config.STOP_MODE() << std::endl;
    exit(-1);
  }

}

void DiagnosticsWorld::SetupStoppingCondition_Generations() {
  stop_run = [this]() {
    return GetUpdate() > config.MAX_GENS();
  };
}

void DiagnosticsWorld::SetupStoppingCondition_Evaluations() {
  stop_run = [this]() {
    return total_test_evaluations > config.MAX_EVALS();
  };
}

void DiagnosticsWorld::SetupEvaluation() {
  std::cout << "Configuring evaluation (mode: " << config.EVAL_MODE() << ")" << std::endl;
  // Total tests is equal to diagnostic dimensionality.
  total_tests = config.DIAGNOSTIC_DIMENSIONALITY();
  // Allocate space for tracking organism aggregate scores
  org_aggregate_scores.clear();
  org_aggregate_scores.resize(config.POP_SIZE(), 0.0);
  // Allocate space for tracking population-wide test coverage
  pop_test_coverage.clear();
  pop_test_coverage.resize(config.DIAGNOSTIC_DIMENSIONALITY(), false);
  // Allocate space for tracking organism test scores
  org_test_scores.clear();
  org_test_scores.resize(
    config.POP_SIZE(),
    emp::vector<double>(total_tests, 0.0)
  );
  // Allocate space for tracking organism test evaluations
  org_test_evaluations.clear();
  org_test_evaluations.resize(
    config.POP_SIZE(),
    emp::vector<bool>(total_tests, false)
  );
  // Allocate space for tracking orgnaism test estimations
  org_test_estimations.clear();
  org_test_estimations.resize(
    config.POP_SIZE(),
    emp::vector<bool>(total_tests, false)
  );
  // Initialize all possible test ids
  possible_test_ids.resize(total_tests, 0);
  std::iota(
    possible_test_ids.begin(),
    possible_test_ids.end(),
    0
  );
  // Initialize all possible population ids
  possible_pop_ids.resize(config.POP_SIZE(), 0);
  std::iota(
    possible_pop_ids.begin(),
    possible_pop_ids.end(),
    0
  );
  // Initialize org group ids
  org_group_ids.clear();
  org_group_ids.resize(config.POP_SIZE(), 0);

  // Clear all actions associated with organism evaluation.
  do_org_evaluation_sig.Clear();
  // First: translate the organism's genome, compute trait info
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < GetSize());
      auto& org = GetOrg(org_id);
      // Translate organism genome into phenotype, compute gene optimality
      org.TranslateGenome(translate_genome_fun);
      org.CalcOptimalTraits(config.TARGET(), config.ACCURACY());
      // Is this better than the current max org fitness seen so far?
      emp_assert(true_max_fit_org_id < GetSize());
      const double max_score = GetOrg(true_max_fit_org_id).GetAggregateScore();
      true_max_fit_org_id = (org.GetAggregateScore() > max_score) ? org_id : true_max_fit_org_id;
      // Update pop-wide trait coverage
      emp_assert(total_tests == org.GetOptimalTraits().size());
      for (size_t i = 0; i < total_tests; ++i) {
        pop_test_coverage[i] = pop_test_coverage[i] || org.IsGeneOptimal(i);
      }
    }
  );
  // Next: reset world organism info
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      // Set all evaluated to false
      org_aggregate_scores[org_id] = 0.0;
      std::fill(
        org_test_scores[org_id].begin(),
        org_test_scores[org_id].end(),
        0.0
      );
      std::fill(
        org_test_evaluations[org_id].begin(),
        org_test_evaluations[org_id].end(),
        false
      );
      std::fill(
        org_test_estimations[org_id].begin(),
        org_test_estimations[org_id].end(),
        false
      );
    }
  );

  // Configure the aggregate score functions
  agg_score_fun_set.clear();
  for (size_t org_id = 0; org_id < config.POP_SIZE(); ++org_id) {
    agg_score_fun_set.emplace_back(
      [this, org_id]() {
        emp_assert(org_id < org_aggregate_scores.size());
        return org_aggregate_scores[org_id];
      }
    );
  }

  // Configure the fitness functions (per-organism, per-test)
  fit_fun_set.clear();
  fit_fun_set.resize(config.POP_SIZE(), emp::vector<std::function<double(void)>>(0));
  for (size_t org_id = 0; org_id < config.POP_SIZE(); ++org_id) {
    for (size_t test_id = 0; test_id < total_tests; ++test_id) {
      fit_fun_set[org_id].emplace_back(
        [this, org_id, test_id]() {
          emp_assert(org_id < org_test_evaluations.size());
          emp_assert(org_id < org_test_scores.size());
          emp_assert(test_id < org_test_evaluations[org_id].size());
          emp_assert(test_id < org_test_scores[org_id].size());
          if (org_test_evaluations[org_id][test_id]) {
            return org_test_scores[org_id][test_id];
          } else {
            return estimate_test_score(org_id, test_id);
          }
        }
      );
    }
  }

  // full vs cohort vs down-sample
  if (config.EVAL_MODE() == "full") {
    SetupEvaluation_Full();
  } else if (config.EVAL_MODE() == "cohort") {
    SetupEvaluation_Cohort();
  } else if (config.EVAL_MODE() == "down-sample") {
    SetupEvaluation_DownSample();
  } else {
    std::cout << "Unknown EVAL_MODE: " << config.EVAL_MODE() << std::endl;
    exit(-1);
  }

  // Record taxon info after evaluation, but before summing aggregate scores
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      auto& org = GetOrg(org_id);
      emp::Ptr<taxon_t> taxon = systematics_ptr->GetTaxonAt(org_id);
      // NOTE - recording true phenotype (not 'evaluated' phenotype)
      taxon->GetData().RecordFitness(org.GetAggregateScore());
      taxon->GetData().RecordPhenotype(
        org.GetPhenotype(),
        org_test_evaluations[org_id]
      );
    }
  );

  // Add signal to calculate aggregate score (using fit funs to incorporate estimator)
  // Need to do this separately to use estimated trait scores
  // NOTE - Cannot use estimator until all organisms have been evaluated and recorded
  // do_org_evaluation_sig.AddAction(
  //   [this](size_t org_id) {
  //     double est_agg_score = 0.0;
  //     for (size_t test_id = 0; test_id < total_tests; ++test_id) {
  //       est_agg_score += fit_fun_set[org_id][test_id]();
  //     }
  //     org_aggregate_scores[org_id] = est_agg_score;
  //   }
  // );

}

void DiagnosticsWorld::SetupEvaluation_Cohort() {
  std::cout << "Configuring evaluation mode: cohort" << std::endl;
  emp_assert(config.NUM_COHORTS() > 0);
  emp_assert(total_tests > 0);
  emp_assert(config.POP_SIZE() > 0);
  const size_t num_cohorts = config.NUM_COHORTS();
  // Compute test cohort sizes
  const size_t base_test_cohort_size = (size_t)(total_tests / num_cohorts);
  size_t leftover_tests = total_tests - (base_test_cohort_size * num_cohorts);
  // Compute organism cohort sizes
  const size_t base_org_cohort_size = (size_t)(config.POP_SIZE() / num_cohorts);
  size_t leftover_orgs = config.POP_SIZE() - (base_org_cohort_size * num_cohorts);

  std::cout << "num_cohorts = " << num_cohorts << std::endl;
  std::cout << "base_test_cohort_size = " << base_test_cohort_size << std::endl;
  std::cout << "leftover_tests = " << leftover_tests << std::endl;

  // Initialize test groupings
  test_groupings.resize(num_cohorts);
  for (size_t i = 0; i < test_groupings.size(); ++i) {
    size_t group_size = base_test_cohort_size;
    if (leftover_tests > 0) {
      ++group_size;
      --leftover_tests;
    }
    auto& test_group = test_groupings[i];
    test_group.group_id = i;
    test_group.Resize(group_size, 0);
    std::cout << "  Test group " << i << " size: " << test_group.member_ids.size() << std::endl;
  }
  emp_assert(leftover_tests == 0);

  std::cout << "base_org_cohort_size = " << base_org_cohort_size << std::endl;
  std::cout << "leftover_orgs = " << leftover_orgs << std::endl;

  // Initialize org groupings
  org_groupings.resize(num_cohorts);
  for (size_t i = 0; i < org_groupings.size(); ++i) {
    size_t group_size = base_org_cohort_size;
    if (leftover_orgs > 0) {
      ++group_size;
      --leftover_orgs;
    }
    auto& org_group = org_groupings[i];
    org_group.group_id = i;
    org_group.Resize(group_size, 0);
    std::cout << "  Org group " << i << " size: " << org_group.member_ids.size() << std::endl;
  }
  emp_assert(leftover_orgs == 0);


  // Setup function to assign test/org groupings
  assign_test_groupings = [this]() {
    // Shuffle all possible test ids
    emp::Shuffle(*random_ptr, possible_test_ids);
    // Assign to cohorts in shuffled order
    size_t cur_pos = 0;
    for (size_t cohort_id = 0; cohort_id < test_groupings.size(); ++cohort_id) {
      auto& cohort = test_groupings[cohort_id];
      emp_assert(cohort.member_ids.size() == cohort.group_size);
      for (size_t test_i = 0; test_i < cohort.group_size; ++test_i) {
        cohort.member_ids[test_i] = possible_test_ids[cur_pos];
        ++cur_pos;
      }
    }
    emp_assert(cur_pos == total_tests);
  };

  // Setup function to assign orgnism groupings
  assign_org_groupings = [this]() {
    // Shuffle all possible test ids
    emp::Shuffle(*random_ptr, possible_pop_ids);
    // Assign to cohorts in shuffled order
    size_t cur_pos = 0;
    for (size_t cohort_id = 0; cohort_id < org_groupings.size(); ++cohort_id) {
      auto& cohort = org_groupings[cohort_id];
      emp_assert(cohort.member_ids.size() == cohort.group_size);
      for (size_t member_i = 0; member_i < cohort.group_size; ++member_i) {
        const size_t pop_id = possible_pop_ids[cur_pos];
        emp_assert(GetOrg(pop_id).GetPopID() == pop_id);
        cohort.member_ids[member_i] = pop_id;
        org_group_ids[pop_id] = cohort.group_id;
        ++cur_pos;
      }
    }
    emp_assert(cur_pos == config.POP_SIZE());
  };

  // Configure organism evaluation (in cohort context)
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < this->GetSize());
      auto& org = this->GetOrg(org_id);
      const size_t group_id = org_group_ids[org_id];
      // Evaluate organism on all tests in appropriate group
      emp_assert(group_id < test_groupings.size());
      auto& test_grouping = test_groupings[group_id];
      auto& cohort_test_ids = test_grouping.member_ids;
      for (size_t i = 0; i < cohort_test_ids.size(); ++i) {
        const size_t test_id = cohort_test_ids[i];
        emp_assert(org.IsEvaluated());
        emp_assert(test_id < org.GetPhenotype().size());
        // Update test score
        org_test_scores[org_id][test_id] = org.GetPhenotype()[test_id];
        // Update evaluated
        org_test_evaluations[org_id][test_id] = true;
        ++total_test_evaluations;
      }
    }
  );

  // TODO print out assigned groupings, make sure makes sense


}

void DiagnosticsWorld::SetupEvaluation_DownSample() {
  std::cout << "Configuring Evaluation mode: down-sample" << std::endl;
  emp_assert(config.TEST_DOWNSAMPLE_RATE() > 0);
  emp_assert(config.TEST_DOWNSAMPLE_RATE() <= 1.0);
  emp_assert(total_tests > 0);

  size_t sample_size = (size_t)(config.TEST_DOWNSAMPLE_RATE() * (double)total_tests);
  sample_size = (sample_size == 0) ? sample_size + 1 : sample_size;
  emp_assert(sample_size > 0);
  emp_assert(sample_size <= total_tests);

  std::cout << "sample_size = " << sample_size << std::endl;

  // Initialize test groupings to one group that will contain random sample
  test_groupings.resize(1);
  auto& test_group = test_groupings.back();
  test_group.group_id = 0;
  test_group.Resize(sample_size, 0);
  emp_assert(test_group.member_ids.size() == sample_size);

  // Setup function to assign random test cases to test group
  assign_test_groupings = [this, sample_size]() {
    emp_assert(test_groupings.size() == 1);
    // Suffle all possible test ids
    emp::Shuffle(*random_ptr, possible_test_ids);
    // NOTE - if wanted to allow over sampling (> num tests), could modify this code to support
    auto& test_group = test_groupings.back();
    emp_assert(test_group.member_ids.size() == sample_size);
    for (size_t i = 0; i < sample_size; ++i) {
      const size_t test_id = possible_test_ids[i];
      test_group.member_ids[i] = test_id;
    }
  };

  // Initialize org groupings to one group that will contain entire population
  org_groupings.resize(1);
  auto& org_group = org_groupings.back();
  org_group.group_id = 0;
  org_group.Resize(config.POP_SIZE(), 0);
  // Go ahead and initialize organism grouping to contain all organism ids
  std::iota(
    org_group.member_ids.begin(),
    org_group.member_ids.end(),
    0
  );
  // Setup function to assign organism groupings (should do nothing, no need to modify current grouping)
  assign_org_groupings = [this]() {
    emp_assert(org_groupings.size() == 1);
    emp_assert(org_groupings.back().member_ids.size() == config.POP_SIZE());
    /* Do nothing */
  };

  // Configure organism evaluation
  do_org_evaluation_sig.AddAction(
    [this, sample_size](size_t org_id) {
      emp_assert(org_id < GetSize());
      emp_assert(test_groupings.size() == 1);
      auto& org = GetOrg(org_id);
      auto& test_group = test_groupings.back();
      emp_assert(org.IsEvaluated());
      emp_assert(test_group.member_ids.size() == sample_size);
      for (size_t i = 0; i < sample_size; ++i) {
        const size_t test_id = test_group.member_ids[i];
        emp_assert(test_id < org.GetPhenotype().size());
        const double test_score = org.GetPhenotype()[test_id];
        // Update test score, aggregate score
        org_test_scores[org_id][test_id] = test_score;
        // Update evaluated
        org_test_evaluations[org_id][test_id] = true;
        ++total_test_evaluations;
      }
    }
  );

  // TODO - print out assigned down sample, test to make sure it works

}

void DiagnosticsWorld::SetupEvaluation_Full() {
  std::cout << "Configuring evaluation mode: full" << std::endl;
  emp_assert(total_tests > 0);

  // Initialize the test groupings with one group that holds all tests
  test_groupings.resize(1);
  auto& test_group = test_groupings.back();
  test_group.group_id = 0;
  test_group.Resize(total_tests, 0);
  std::iota(
    test_group.member_ids.begin(),
    test_group.member_ids.end(),
    0
  );
  // Setup function to assign test cases to test group (should do nothing to change test group)
  assign_test_groupings = [this]() {
    emp_assert(test_groupings.size() == 1);
    emp_assert(test_groupings.back().member_ids.size() == total_tests);
    /* Do nothing */
  };

  // Initialize org groupings to one group containing all organisms
  org_groupings.resize(1);
  auto& org_group = org_groupings.back();
  org_group.group_id = 0;
  org_group.Resize(config.POP_SIZE(), 0);
  // Go ahead and initialize organism grouping to contain all organism ids
  std::iota(
    org_group.member_ids.begin(),
    org_group.member_ids.end(),
    0
  );
  // Setup function to assign organism groupings (should do nothing, no need to modify current grouping)
  assign_org_groupings = [this]() {
    emp_assert(org_groupings.size() == 1);
    emp_assert(org_groupings.back().member_ids.size() == config.POP_SIZE());
    /* Do nothing */
  };

  // Configure organism evaluation
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < GetSize());
      emp_assert(test_groupings.size() == 1);
      auto& org = GetOrg(org_id);
      emp_assert(org.IsEvaluated());
      emp_assert(test_groupings.back().member_ids.size() == total_tests);
      for (size_t test_id = 0; test_id < total_tests; ++test_id) {
        emp_assert(test_id < org.GetPhenotype().size());
        const double test_score = org.GetPhenotype()[test_id];
        // Update test score, aggregate score
        org_test_scores[org_id][test_id] = test_score;
        // Update evaluated
        org_test_evaluations[org_id][test_id] = true;
        ++total_test_evaluations;
      }
    }
  );

  // TODO - test full evaluation to make sure it works

}

void DiagnosticsWorld::SetupFitFunEstimator() {
  // Setup the default estimate_test_score functionality
  // TODO - setup configurable fitness estimation
  std::cout << "Configuring fitness function estimator (mode: " << config.EVAL_FIT_EST_MODE() << ")" << std::endl;

  bool estimation_mode = config.EVAL_FIT_EST_MODE() != "none";
  if (config.EVAL_FIT_EST_MODE() == "none") {
    SetupFitFunEstimator_None();
  } else if (config.EVAL_FIT_EST_MODE() == "ancestor") {
    SetupFitFunEstimator_Ancestor();
  } else if (config.EVAL_FIT_EST_MODE() == "relative") {
    SetupFitFunEstimator_Relative();
  } else if (config.EVAL_FIT_EST_MODE() == "ancestor-opt") {
    SetupFitFunEstimator_AncestorOpt();
  } else if (config.EVAL_FIT_EST_MODE() == "relative-opt") {
    SetupFitFunEstimator_RelativeOpt();
  } else {
    std::cout << "Unrecognized EVAL_FIT_EST_MODE: " << config.EVAL_FIT_EST_MODE() << std::endl;
    exit(-1);
  }

  // If we're using trait estimation, we can use all traits during selection
  // otherwise, we can only use group traits.
  if (estimation_mode) {
    // run_selection_routine
    run_selection_routine = [this]() {
      // Resize parent ids to hold pop_size parents
      selected_parent_ids.resize(config.POP_SIZE(), 0);
      emp_assert(test_groupings.size() == org_groupings.size());
      const size_t num_groups = org_groupings.size();
      // For each grouping, select a number of parents equal to group size
      size_t num_selected = 0;
      for (size_t group_id = 0; group_id < num_groups; ++group_id) {
        auto& org_group = org_groupings[group_id];
        const size_t n = org_group.GetSize();
        // Run selection, but use all possible test ids
        auto& selected = selection_fun(
          n,
          org_group.member_ids,
          possible_test_ids
        );
        emp_assert(selected.size() == n);
        emp_assert(n + num_selected <= selected_parent_ids.size());
        std::copy(
          selected.begin(),
          selected.end(),
          selected_parent_ids.begin() + num_selected
        );
        num_selected += n;
      }
    };
  } else {
    // No estimation, so only use evaluated tests in selection routine
    run_selection_routine = [this]() {
      // Resize parent ids to hold pop_size parents
      selected_parent_ids.resize(config.POP_SIZE(), 0);
      emp_assert(test_groupings.size() == org_groupings.size());
      const size_t num_groups = org_groupings.size();
      // For each grouping, select a number of parents equal to group size
      size_t num_selected = 0;
      for (size_t group_id = 0; group_id < num_groups; ++group_id) {
        auto& org_group = org_groupings[group_id];
        auto& test_group = test_groupings[group_id];
        const size_t n = org_group.GetSize();
        auto& selected = selection_fun(
          n,
          org_group.member_ids,
          test_group.member_ids
        );
        emp_assert(selected.size() == n);
        emp_assert(n + num_selected <= selected_parent_ids.size());
        std::copy(
          selected.begin(),
          selected.end(),
          selected_parent_ids.begin() + num_selected
        );
        num_selected += n;
      }
    };

  }
}

void DiagnosticsWorld::SetupFitFunEstimator_None() {
  // Don't estimate anything
  estimate_test_score = [this](size_t org_id, size_t test_id) {
    return org_test_scores[org_id][test_id];
  };
}

void DiagnosticsWorld::SetupFitFunEstimator_Ancestor() {

  // estimate_test_score
  estimate_test_score = [this](size_t org_id, size_t test_id) {
    emp::Ptr<taxon_t> taxon_ptr = systematics_ptr->GetTaxonAt(org_id);

    phylo::TraitEstInfo& est_info = phylo::NearestAncestorWithTraitEval(
      taxon_ptr,
      test_id,
      (size_t)config.EVAL_MAX_PHYLO_SEARCH_DEPTH()
    );

    if (est_info.estimate_success) {
      org_test_estimations[org_id][test_id] = true;
      ++total_test_estimations;
      return est_info.estimated_score;
    } else {
      return org_test_scores[org_id][test_id];
    }

  };

}

void DiagnosticsWorld::SetupFitFunEstimator_AncestorOpt() {
  // estimate_test_score
  estimate_test_score = [this](size_t org_id, size_t test_id) {
    emp::Ptr<taxon_t> taxon_ptr = systematics_ptr->GetTaxonAt(org_id);

    // If taxon has already been estimated on this trait, re-use estimated value
    if (taxon_ptr->GetData().GetTraitEstimationInfo(test_id).estimated) {
      org_test_estimations[org_id][test_id] = true;
      return taxon_ptr->GetData().GetTraitEstimationInfo(test_id).estimated_score;
    }

    // Otherwise, find nearest ancestor
    phylo::TraitEstInfo& est_info = phylo::NearestAncestorWithTraitEvalOpt(
      taxon_ptr,
      test_id,
      (size_t)config.EVAL_MAX_PHYLO_SEARCH_DEPTH()
    );

    if (est_info.estimate_success) {
      org_test_estimations[org_id][test_id] = true;
      ++total_test_estimations;
    }

    return est_info.estimated_score;
  };
}

void DiagnosticsWorld::SetupFitFunEstimator_Relative() {
  // estimate_test_score
  estimate_test_score = [this](size_t org_id, size_t test_id) {
    emp::Ptr<taxon_t> taxon_ptr = systematics_ptr->GetTaxonAt(org_id);

    phylo::TraitEstInfo& est_info = phylo::NearestRelativeWithTraitEval(
      taxon_ptr,
      test_id,
      (size_t)config.EVAL_MAX_PHYLO_SEARCH_DEPTH()
    );

    if (est_info.estimate_success) {
      org_test_estimations[org_id][test_id] = true;
      ++total_test_estimations;
      return est_info.estimated_score;
    } else {
      return org_test_scores[org_id][test_id];
    }
  };

}

void DiagnosticsWorld::SetupFitFunEstimator_RelativeOpt() {
  // estimate_test_score
  estimate_test_score = [this](size_t org_id, size_t test_id) {
    emp::Ptr<taxon_t> taxon_ptr = systematics_ptr->GetTaxonAt(org_id);

    // If taxon has already been estimated on this trait, re-use estimated value
    if (taxon_ptr->GetData().GetTraitEstimationInfo(test_id).estimated) {
      org_test_estimations[org_id][test_id] = true;
      return taxon_ptr->GetData().GetTraitEstimationInfo(test_id).estimated_score;
    }

    phylo::TraitEstInfo& est_info = phylo::NearestRelativeWithTraitEvalOpt(
      taxon_ptr,
      test_id,
      (size_t)config.EVAL_MAX_PHYLO_SEARCH_DEPTH()
    );

    if (est_info.estimate_success) {
      org_test_estimations[org_id][test_id] = true;
      ++total_test_estimations;
    }

    return est_info.estimated_score;
  };
}

void DiagnosticsWorld::SetupSelection() {
  std::cout << "Configuring parent selection routine" << std::endl;
  emp_assert(selector == nullptr);

  if (config.SELECTION() == "lexicase" ) {
    SetupSelection_Lexicase();
  } else if (config.SELECTION() == "tournament" ) {
    SetupSelection_Tournament();
  } else if (config.SELECTION() == "truncation" ) {
    SetupSelection_Truncation();
  } else if (config.SELECTION() == "none" ) {
    SetupSelection_None();
  } else if (config.SELECTION() == "random" ) {
    SetupSelection_Random();
  } else {
    std::cout << "Unknown selection scheme: " << config.SELECTION() << std::endl;
    exit(-1);
  }

}

void DiagnosticsWorld::SetupSelection_Lexicase() {
  // TODO - test lexicase
  selector = emp::NewPtr<selection::LexicaseSelect>(
    fit_fun_set,
    *random_ptr
  );

  selection_fun = [this](
    size_t n,
    const emp::vector<size_t>& org_group,
    const emp::vector<size_t>& test_group
  ) -> emp::vector<size_t>& {
    // Cast selector to lexicase selection
    auto& sel = *(selector.Cast<selection::LexicaseSelect>());
    return sel(n, org_group, test_group);
  };

}

void DiagnosticsWorld::SetupSelection_Tournament() {
  selector = emp::NewPtr<selection::TournamentSelect>(
    agg_score_fun_set,
    *random_ptr,
    config.TOURNAMENT_SIZE()
  );

  selection_fun = [this](
    size_t n,
    const emp::vector<size_t>& org_group,
    const emp::vector<size_t>& test_group
  ) -> emp::vector<size_t>& {
    // Cast selector to lexicase selection
    auto& sel = *(selector.Cast<selection::TournamentSelect>());
    return sel(n, org_group);
  };
}

void DiagnosticsWorld::SetupSelection_Truncation() {
  // TODO - setup truncation
  emp_assert(false);
}

void DiagnosticsWorld::SetupSelection_None() {
  // TODO - setup lexicase
  emp_assert(false);
}

void DiagnosticsWorld::SetupSelection_Random() {
  // TODO - setup lexicase
  emp_assert(false);
}


void DiagnosticsWorld::InitializePopulation() {

  std::cout << "Initializing population" << std::endl;

  // TODO - root the phylogenetic tree

  if (config.INIT_POP_RAND()) {
    for (size_t i = 0; i < config.POP_SIZE(); ++i) {
      Inject(
        emp::RandomDoubleVector(
          *random_ptr,
          config.DIAGNOSTIC_DIMENSIONALITY(),
          config.GENE_LOWER_BND(),
          config.GENE_UPPER_BND()
        ),
        1
      );
    }
  } else {
    org_t default_org(config.DIAGNOSTIC_DIMENSIONALITY(), 0.0);
    Inject(default_org.GetGenome(), config.POP_SIZE());
  }

}

void DiagnosticsWorld::SnapshotConfig() {
  emp::DataFile snapshot_file(output_dir + "run_config.csv");
  std::function<std::string(void)> get_param;
  std::function<std::string(void)> get_value;
  snapshot_file.AddFun<std::string>(
    [&get_param]() { return get_param(); },
    "parameter"
  );
  snapshot_file.AddFun<std::string>(
    [&get_value]() { return get_value(); },
    "value"
  );
  snapshot_file.PrintHeaderKeys();

  for (const auto& entry : config) {
    get_param = [&entry]() { return entry.first; };
    get_value = [&entry]() { return emp::to_string(entry.second->GetValue()); };
    snapshot_file.Update();
  }
}


}