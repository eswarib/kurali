# Title

Speech-to-Well formatted text Transformation: A Pipeline Combining ASR (Automatic Speech Recognition), Inverse Text Normalization and/or Small Language Models

---

# Abstract

This paper presents a practical and modular pipeline for converting spoken dictation into professionally formatted text. Depending on the use of the final text output (E-Mail, technical documentation, any other specific use case...), the system integrates Automatic Speech Recognition (ASR), Inverse Text Normalization (ITN) and/or Small Language Models (SLMs) for stylistic rewriting. We explore model choices for low-latency environments, and identify gaps in current research where end-to-end spoken-to-formatted text systems remain underexplored.

---

# 1. Introduction

Voice-driven interfaces are increasingly used for productivity tasks such as email composition. However, raw ASR outputs are not suitable for direct use in professional communication due to lack of structure, punctuation, and formality.

This work explores the problem of transforming spoken dictation into structured, professional text by combining multiple natural language processing components into a unified pipeline.

---

# 2. Problem Statement

The task can be defined as:

> "Given a spoken input, generate a well-structured, grammatically correct, and professionally formatted email/text preserving the original intent."

The Whisper model is used for speech-to-raw-text conversion.

Challenges include:

- Lack of punctuation and casing in Whisper model output
- Ambiguity in spoken expressions (numbers, dates, intent)
- Need for tone adjustment (informal → formal)
- Maintaining semantic fidelity while rewriting

---

# 3. System Overview

## 3.1 Pipeline Architecture

1. Whisper model — to produce raw text from speech
2. Text rewriting via Inverse Text Normalization (ITN) / Small Language Model (SLM)

## 3.2 Design Rationale

- Modular design improves interpretability and debugging
- Separation of concerns reduces hallucination risk
- Enables independent optimization of each component

---

# 4. Inverse Text Normalization (ITN)

## 4.1 Definition

ITN converts spoken-form text into structured written form.

Example:

"twenty twenty-six" → "2026"

## 4.2 Approaches

### Rule-Based (WFST)

- Deterministic
- High precision
- Difficult to scale

### Neural Approaches

- Sequence-to-sequence models
- Tagging-based models

## 4.3 Trade-offs

| Approach | Pros | Cons |
|----------|------|------|
| Rule-based | Reliable | Rigid |
| Neural | Flexible | Risk of errors |

---

# 5. Text Rewriting for professional text format

## 5.1 Task Definition

Transform informal or unstructured text into professional text format; email is one use case.

## 5.2 Related Research Areas

- Text Style Transfer
- Paraphrasing
- Controlled Text Generation
- Instruction-Tuned Language Models

## 5.3 Requirements

- Add greeting and closing
- Improve grammar and clarity
- Adjust tone (polite, professional)
- Preserve intent

---

# 6. Methodology

## 6.1 Overview

To produce the final formatted text, we have two approaches:

1. Use ITN and SLM
2. Use only an SLM

## 6.2 Using Inverse Text Normalization (ITN) and SLM

### 6.2.1 Approach

We can adopt a hybrid ITN strategy:

- Rule-based patterns for high-precision transformations (numbers, dates, currency)
- Lightweight tagging heuristics for token-level corrections

### 6.2.2 Examples

- "twenty twenty six" → "2026"
- "five pounds fifty" → "£5.50"

### 6.2.3 Rationale

Rule-based ITN ensures deterministic behavior for critical entities, reducing hallucination risk compared to fully generative approaches (Sproat et al., 2001; Bakhturina et al., 2022). Lightweight tagging heuristics support token-level corrections.

## 6.3 SLM-based text formatting

### 6.3.1 Model Choice

We evaluate compact transformer-based models (e.g., 2B–8B parameters) quantized for local inference. Models are instruction-tuned to follow formatting constraints.

### 6.3.2 Prompt Design

Structured prompt:

"Convert the following dictation into a professional email. Add greeting and closing, fix grammar, and keep meaning unchanged."

### 6.3.3 Constraints

- Preserve semantic intent
- Improve grammar and punctuation
- Enforce polite, professional tone

### 6.3.4 Output Template

- Greeting
- Body (rewritten content)
- Closing

---

# 7. Evaluation

## 7.1 Metrics

### 7.1.1 ITN Evaluation

*(Add criteria, datasets, or protocols here.)*

### 7.1.2 Rewriting Evaluation

- Grammaticality (human rating)
- Formality (Rao & Tetreault, 2018)
- Semantic similarity (BLEU / ROUGE as proxy)

---

# 8. Fine-tuning SLMs

*(Section body.)*

---

# 9. Conclusion

This work outlines research relevant to ITN- and/or SLM-based text reformatting.

---

# 10. References

## Web and preprints

- Distilling Step-by-Step (ResearchGate): <https://www.researchgate.net/publication/372916020_Distilling_Step-by-Step_Outperforming_Larger_Language_Models_with_Less_Training_Data_and_Smaller_Model_Sizes>  
  Some authors are from Google; open-access availability varies by venue.

- Towards an On-device Agent for Text Rewriting: <https://arxiv.org/pdf/2308.11807>

- RewriteLM (ResearchGate): <https://www.researchgate.net/publication/371041356_RewriteLM_An_Instruction-Tuned_Large_Language_Model_for_Text_Rewriting>

- Additional preprint: <https://arxiv.org/pdf/2305.15685>

## Industry-aligned references (with links)

### Google (text-to-text + rewriting)

1. **T5** — Text-to-Text Transfer Transformer (core to a text-to-text design). T5 paper (JMLR). Key idea: unify NLP tasks as input → output text.

2. **mT5** (multilingual extension): <https://arxiv.org/abs/2010.11934> — extends T5 to 101 languages.

### Microsoft (speech + rewriting + control)

3. **SpeechT5** (speech ↔ text): <https://arxiv.org/abs/2110.07205> — joint modeling of speech and text in one framework.

4. **Thutmose-style / NeMo-style ITN** — industry often prefers tagging models over generation to limit hallucinations in ITN.

5. **Content Transfer for Controlled Generation** (Microsoft Research): <https://www.microsoft.com/en-us/research/wp-content/uploads/2019/06/1905.05293.pdf> — generate text while preserving content but changing form/style.

6. **Contextual Text Style Transfer** (Microsoft Research): <https://www.microsoft.com/en-us/research/group/natural-language-processing/publications/>

7. **Meta-DiffuB** (2024): <https://www.microsoft.com/en-us/research/publication/meta-diffub-a-contextualized-sequence-to-sequence-text-diffusion-model-with-meta-exploration/> — sequence-to-sequence generation with diffusion.

## Bibliography (short)

- Sproat, R., Black, A. W., Chen, S., Kumar, S., Ostendorf, M., & Richards, C. (2001). Normalization of non-standard words. *Computer Speech & Language.*
- Zhang, Y., et al. (2019). Neural models for inverse text normalization. *Interspeech.*
- Bakhturina, E., et al. (2022). Thutmose Tagger: Single-pass neural model for inverse text normalization. NVIDIA Research.
- NVIDIA (2021). NeMo Inverse Text Normalization.
- AdapITN (2023). Adaptive inverse text normalization system.
- Rao, S., & Tetreault, J. (2018). Dear Sir or Madam: Formality in text. *NAACL.*
- Krishna, K., et al. (2020). Reformulating unsupervised style transfer as paraphrase generation. *EMNLP.*
- Raffel, C., et al. (2020). Exploring the limits of transfer learning with a unified text-to-text transformer. *JMLR.*
- Tilk, O., & Alumäe, T. (2016). Bidirectional recurrent neural network with attention for punctuation restoration. *Interspeech.*
- Gliwa, B., et al. (2019). SAMSum corpus: A human-annotated dialogue dataset for abstractive summarization. *EMNLP.*

---

# 11. Research Strategy and Search Keywords

## 11.1 Overview

To identify relevant literature, we conducted targeted searches on ResearchGate and related academic platforms. Given the interdisciplinary nature of the problem, queries were designed to cover multiple subfields including ASR post-processing, inverse text normalization, and text rewriting.

## 11.2 Keyword Categories

### A. Inverse Text Normalization (ITN)

- "inverse text normalization ASR"
- "neural ITN transformer"
- "ITN tagging model"
- "spoken to written normalization"

### B. Text Rewriting and Style Transfer

- "text style transfer formal writing"
- "paraphrasing for formality"
- "controlled text generation"
- "instruction tuned text rewriting"
- "instruction tuning"
- "style transfer"
- "paraphrasing"
- "controlled generation"

### C. ASR Post-processing

- "ASR punctuation restoration"
- "speech recognition text post-processing"

### D. End-to-End and Pipeline Systems

- "speech to structured text generation"
- "ASR post processing text rewriting"
- "spoken dialogue to email generation"

## 11.3 Search Strategy

- Initial keyword-based search
- Exploration of citation graphs (forward and backward citations)
- Identification of frequently cited foundational papers

## 11.4 Key Insight

No single body of work directly addresses spoken-to-formatted text transformation. Instead, relevant research is distributed across multiple domains.

---

# Appendix

## A.1 Inverse Text Normalization (ITN)

Early ITN systems rely on Weighted Finite State Transducers (WFSTs) for deterministic mapping from spoken to written forms (Sproat et al., 2001). While highly precise, these systems are brittle and require extensive manual engineering.

Recent work explores neural approaches. Zhang et al. (2019) propose sequence-to-sequence models for ITN, treating the task as translation. Bakhturina et al. (2022) introduce tagging-based models (e.g., Thutmose Tagger), which reduce hallucination by labeling tokens instead of generating text. NVIDIA NeMo (2021) presents a production-grade hybrid system combining rules and neural models for robustness.

Adaptive approaches such as AdapITN (2023) focus on improving efficiency and domain adaptability, showing that hybrid systems remain competitive in real-world deployments.

## A.2 Text Style Transfer and Rewriting

Text rewriting for formality and politeness is widely studied under style transfer. Rao and Tetreault (2018) introduce datasets and baselines for formality transfer. Krishna et al. (2020) explore paraphrasing as a controlled generation task.

Modern approaches rely on instruction-tuned language models. Models such as T5 (Raffel et al., 2020) and GPT-style architectures demonstrate strong performance in rewriting tasks using prompt-based control rather than explicit supervision.

## A.3 ASR Post-processing

ASR outputs typically require punctuation restoration and capitalization. Tilk and Alumäe (2016) use recurrent models for punctuation prediction, while more recent transformer-based approaches improve robustness.

End-to-end systems that combine ASR with downstream NLP tasks remain limited. Most production systems adopt a modular pipeline separating recognition, normalization, and generation.

## A.4 Email and Dialogue Generation

Email generation has been explored as a form of task-oriented text generation. Studies on dialogue summarization (Gliwa et al., 2019) and intent-to-text generation provide relevant foundations, though direct spoken-to-email transformation remains underexplored.
